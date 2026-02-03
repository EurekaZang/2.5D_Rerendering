# Project Prompt (English) — RGBD Re-rendering With Different Focal Lengths (Same Viewpoint)

You are GitHub Copilot. Help me build an Ubuntu 22.04 LTS engineering project that takes **one RGB image + a pixel-aligned metric depth map (meters) + camera intrinsics (fx, fy, cx, cy)** and produces a **series of re-rendered RGB images and metric depth maps** as if captured from the **same camera pose** but with **different focal lengths** (both wider/shorter and longer/telephoto).  
We only need **visible regions from the original view**; output a **validity mask** for pixels that are actually rendered. **No inpainting/hole filling** required.

---

## 1) Core Requirements

### Inputs
- `rgb` : HxWx3 (uint8 or float), the source color image.
- `depth` : HxW (float32 recommended), **metric depth in meters**, aligned to `rgb` pixels. Depth is **camera Z** (distance along optical axis). Invalid depths may be 0 / NaN / Inf.
- `K` : source intrinsics  
  \[
  K = \begin{bmatrix}
  f_x & 0 & c_x \\
  0 & f_y & c_y \\
  0 & 0 & 1
  \end{bmatrix}
  \]
- A list of target intrinsics `K'` where **only focal length changes** (`fx'`, `fy'`), optionally principal point `cx'`, `cy'` (default: keep same as source).
- Target resolution `(W', H')` (default: same as source unless specified).

### Outputs for each target focal setting
- `rgb_out` : H'xW'x3 (same type as desired)
- `depth_out` : H'xW' float32, **metric depth (meters)** = camera-space Z
- `mask_out` : H'xW' uint8/bool (1 where a triangle wrote the pixel via depth test, else 0)

### Constraints / Assumptions
- **Camera pose is unchanged** (no rotation/translation). Only `K -> K'` changes.
- Only surfaces visible in the source RGBD can be rendered. New areas (revealed by wide FOV) remain invalid (mask=0).
- Must handle occlusions correctly (z-buffer).
- Must avoid “rubber-sheet” artifacts across depth discontinuities.

---

## 2) Recommended Algorithm (Route B): Depth-to-Mesh + Rasterization (GPU)

Use a **2.5D mesh** built from the depth map (grid triangulation), with **edge breaking** at depth discontinuities. Render that mesh using GPU rasterization.

### 2.1 Back-project depth to 3D vertices (camera space)
For each pixel `(u, v)` with valid depth `z = depth[v,u]`:
- Camera-space vertex:
  \[
  X = (u - c_x)\cdot z / f_x,\quad
  Y = (v - c_y)\cdot z / f_y,\quad
  Z = z
  \]
Store vertex `V[u,v] = (X,Y,Z)`.

### 2.2 Attach texture coordinates (recommended)
Use the original RGB as a texture. For each vertex:
- `uv = ((u + 0.5)/W, (v + 0.5)/H)` (or `(u/W, v/H)` depending on sampling convention)
Then fragment shader samples the RGB texture.

### 2.3 Build triangle indices (grid triangulation)
For each quad cell with pixel corners:
- `v00 = (u,v)`
- `v10 = (u+1,v)`
- `v01 = (u,v+1)`
- `v11 = (u+1,v+1)`
Two triangles:
- `tri1: v00, v10, v11`
- `tri2: v00, v11, v01`

### 2.4 Break edges at depth discontinuities (CRITICAL)
Do **not** create triangles that span across depth jumps (foreground/background).
Use a combination of relative + absolute thresholds; e.g.:
- Relative: `abs(za - zb) / min(za, zb) > tau_rel`  => break
- Absolute: `abs(za - zb) > tau_abs` => break
Typical starting points:
- `tau_rel` in `[0.02, 0.10]` depending on depth noise
- `tau_abs` in `[0.05m, 0.20m]` depending on scene scale

For each candidate triangle, check its 3 edges; if any edge violates continuity, skip that triangle.

---

## 3) Rendering Architecture (Best Fit): C++ + OpenGL 4.6 + EGL (Headless) + FBO MRT

### Why
- Fast and stable on NVIDIA A6000.
- Hardware z-buffer, perspective-correct interpolation, triangle coverage with minimal cracks.
- Headless operation via EGL for server/CLI pipelines.
- Multi-Render-Target (MRT) to output RGB + metric depth + mask in one pass.

### Buffers / Targets
- Depth test buffer: standard GL depth buffer (for visibility).
- Color targets (FBO attachments):
  1. `RGB` : `GL_RGBA8` (or `GL_SRGB8_ALPHA8` if you want sRGB pipeline)
  2. `Metric depth` : `GL_R32F` (store camera Z in meters)
  3. `Mask` : `GL_R8` (write 1 for rendered fragments)

### Shaders (concept)
- Vertex shader: takes camera-space vertex `V=(X,Y,Z)` and outputs clip-space position for `K'`.
- Fragment shader:
  - `rgb_out = texture(rgbTex, uv)`
  - `depth_out = Z` (metric depth)
  - `mask_out = 1`

IMPORTANT: Do **not** read back the default depth buffer as metric depth (it is non-linear). Always output metric Z to an `R32F` color attachment.

---

## 4) Camera Projection Details (K' to GL clip space)

We have camera-space points `(X,Y,Z)` already. For target intrinsics `K'`:
\[
u' = f_x' X/Z + c_x',\quad
v' = f_y' Y/Z + c_y'
\]

To render via OpenGL, implement one of these approaches:

### Approach A (Simple): Build a custom projection matrix from K'
Create an OpenGL-compatible projection matrix using `fx', fy', cx', cy'`, image size `W', H'`, and `near/far`.  
Be careful about image coordinate conventions (OpenGL NDC has y-up; image v is y-down). Include a y-flip if necessary.

### Approach B (Alternative): Do manual projection in vertex shader
Compute normalized device coordinates directly:
- Convert `(u', v')` into NDC:
  - `x_ndc = 2*(u'/W') - 1`
  - `y_ndc = 1 - 2*(v'/H')`  (flip y)
- For z, you can use standard `gl_Position.z` based on `near/far` mapping, but still write metric Z to the `R32F` attachment.

Copilot should implement Approach A or B correctly and consistently.

---

## 5) Pipeline Steps (End-to-End)

1. Load `rgb` and `depth` (depth in meters, float32 preferred).
2. Validate depth; mark invalid pixels.
3. Build vertex array `V` (camera-space) and UV array.
4. Build index buffer for triangles, skipping triangles across depth discontinuities.
5. Initialize EGL + OpenGL context (headless).
6. Upload:
   - Vertex buffer (VBO): positions + UVs
   - Index buffer (EBO)
   - RGB texture
7. For each target focal setting `(fx', fy')`:
   - Create `K'` (keep `cx, cy` unless user overrides)
   - Set projection (uniform matrix or parameters)
   - Bind FBO with MRT textures (RGB, metric depth, mask)
   - Clear: RGB=0, depthOut=+inf (or 0), mask=0; clear GL depth buffer
   - Render mesh with depth test enabled
   - Read back MRT textures to CPU and save:
     - RGB as PNG/JPEG
     - metric depth as EXR/NPY (float)
     - mask as PNG (8-bit)
8. Provide a CLI:
   - `--rgb path --depth path --fx --fy --cx --cy --out_dir`
   - `--focal_list` or `--zoom_range` (e.g., scales `[0.5, 0.75, 1.0, 1.5, 2.0]`)
   - `--tau_rel --tau_abs`
   - optional `--W_out --H_out`

---

## 6) Deliverables / Code Structure

- Language: C++17 (or newer)
- Modules:
  - `io/` image + depth loading/saving (OpenCV + OpenEXR or cnpy for NPY)
  - `mesh/` vertex generation + triangle generation with edge breaking
  - `render/` EGL context + GL setup, shader compilation, FBO/MRT creation, rendering loop
  - `app/` CLI + config parsing (cxxopts or argparse-like)
- Provide a `CMakeLists.txt` for Ubuntu 22.04.
- Include a small test that:
  - loads sample RGBD
  - renders with multiple focal scales
  - verifies mask non-empty, depth reasonable range, outputs generated files.

---

## 7) Notes / Non-Goals
- No hallucinated geometry beyond original visibility; do not attempt multi-view completion.
- No hole filling/inpainting; mask indicates valid pixels.
- If small 1-pixel cracks appear, that’s acceptable (but triangulation + proper edge breaking should minimize them).
- Focus only on changing intrinsics (focal length). Extrinsics remain identity.

---

## 8) Expected Behavior
- Decreasing `fx'/fy'` (wider FOV) should show more area *only if it was already in the mesh coverage*; otherwise those pixels remain invalid (mask=0).
- Increasing `fx'/fy'` (telephoto) should effectively zoom in; fewer pixels will be valid at the periphery, but the rendered region should be sharp and occlusion-correct.
- Output metric depth should match the rendered visible surface Z in meters.

---

## 9) Implementation Tips
- Use `GL_R32F` for metric depth output attachment.
- Use `GL_NEAREST` or `GL_LINEAR` sampling for RGB texture; choose consistent UV convention.
- Beware of the principal point convention and pixel-center offsets (`+0.5`); keep it consistent between back-projection and rendering.
- Keep an option to pre-filter depth (e.g., bilateral) if noise causes too many broken triangles.

Build the project accordingly.