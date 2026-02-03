# RGBD Rerendering with Variable Focal Lengths

GPU 加速的 RGBD 图像重渲染工具，支持使用不同焦距（从同一视点）重新渲染 RGB 图像和深度图。

## 特性

- **GPU 加速渲染**：使用 OpenGL 4.6 + EGL 进行无头（headless）GPU 渲染
- **2.5D 网格重建**：从深度图生成三角网格，并在深度不连续处断开边缘
- **多渲染目标（MRT）**：单次渲染同时输出 RGB、深度和有效性掩码
- **灵活的输入/输出**：支持 PNG、EXR、NPY 等多种格式
- **命令行接口**：易于集成到自动化流水线中

## 系统要求

- Ubuntu 22.04 LTS
- NVIDIA GPU（支持 OpenGL 4.3+）
- NVIDIA 驱动程序
- CMake 3.20+

## 安装

### 1. 安装依赖

```bash
# 运行安装脚本
chmod +x scripts/install_dependencies.sh
./scripts/install_dependencies.sh
```

或者手动安装：

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake ninja-build \
    libgl1-mesa-dev libegl1-mesa-dev libgles2-mesa-dev \
    libopengl-dev libopencv-dev libopenexr-dev
```

### 2. 构建项目

```bash
# 使用构建脚本
chmod +x scripts/build.sh
./scripts/build.sh

# 或手动构建
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 3. 运行测试

```bash
./build/bin/test_rerender
```

## 使用方法

### 基本用法

```bash
./build/bin/rgbd_rerender \
    --rgb path/to/rgb.png \
    --depth path/to/depth.npy \
    --fx 500 --fy 500 \
    --focal_list 0.5,0.75,1.0,1.5,2.0 \
    --out_dir output
```

### 完整参数列表

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--rgb` | RGB 图像路径 | 必填 |
| `--depth` | 深度图路径 | 必填 |
| `--fx` | 焦距 X（像素） | 必填 |
| `--fy` | 焦距 Y（像素） | 必填 |
| `--cx` | 主点 X | 图像中心 |
| `--cy` | 主点 Y | 图像中心 |
| `--out_dir` | 输出目录 | `./output` |
| `--depth_scale` | 深度缩放因子（转换为米） | 1.0 |
| `--focal_list` | 焦距缩放比例列表 | 0.5,0.75,1.0,1.5,2.0 |
| `--tau_rel` | 相对深度阈值 | 0.05 |
| `--tau_abs` | 绝对深度阈值（米） | 0.1 |
| `--near` | 近裁剪面 | 0.1 |
| `--far` | 远裁剪面 | 100.0 |
| `--gpu` | GPU 设备索引 | -1（自动） |
| `--W_out` | 输出宽度 | 同输入 |
| `--H_out` | 输出高度 | 同输入 |

### 深度图格式

支持的深度图格式：
- **PNG（16位）**：使用 `--depth_scale 0.001` 将毫米转换为米
- **NPY**：NumPy 二进制格式，float32
- **EXR**：OpenEXR 格式，float32

### 输出文件

对于每个焦距缩放比例，生成以下文件：
- `scale_X.XX_rgb.png`：重渲染的 RGB 图像
- `scale_X.XX_depth.png`：重渲染的深度图（16位，毫米）
- `scale_X.XX_depth.exr`：重渲染的深度图（float32，米）
- `scale_X.XX_mask.png`：有效性掩码（白色=有效）

## 示例

### 运行演示

```bash
chmod +x scripts/run_demo.sh
./scripts/run_demo.sh
```

### 使用毫米深度图

```bash
./build/bin/rgbd_rerender \
    --rgb image.png \
    --depth depth_mm.png \
    --depth_scale 0.001 \
    --fx 525 --fy 525
```

### 自定义阈值

```bash
./build/bin/rgbd_rerender \
    --rgb image.png \
    --depth depth.npy \
    --fx 525 --fy 525 \
    --tau_rel 0.03 --tau_abs 0.05
```

## 算法原理

### 1. 深度图到网格转换

将每个有效深度像素反投影到 3D 相机空间：
```
X = (u - cx) * z / fx
Y = (v - cy) * z / fy
Z = z
```

### 2. 三角形生成与边缘断裂

对于每个像素四边形，生成两个三角形。在深度不连续处断开边缘：
- 相对阈值：`|z1 - z2| / min(z1, z2) > tau_rel`
- 绝对阈值：`|z1 - z2| > tau_abs`

### 3. GPU 光栅化

使用 OpenGL 渲染网格，通过多渲染目标（MRT）同时输出：
- RGB（从纹理采样）
- 度量深度（相机 Z 坐标，米）
- 有效性掩码

## 目录结构

```
.
├── CMakeLists.txt
├── README.md
├── include/              # 头文件
│   ├── types.hpp
│   ├── image_io.hpp
│   ├── depth_io.hpp
│   ├── mesh_generator.hpp
│   ├── depth_mesh.hpp
│   ├── egl_context.hpp
│   ├── shader.hpp
│   ├── framebuffer.hpp
│   ├── gl_renderer.hpp
│   └── config.hpp
├── src/                  # 源文件
│   ├── io/
│   ├── mesh/
│   ├── render/
│   ├── app/
│   └── main.cpp
├── shaders/              # GLSL 着色器
│   ├── mesh.vert
│   └── mesh.frag
├── test/                 # 测试代码
├── scripts/              # 脚本
└── external/             # 外部依赖（GLAD）
```

## 故障排除

### EGL 初始化失败

1. 确保安装了 NVIDIA 驱动：
   ```bash
   nvidia-smi
   ```

2. 检查 EGL 库：
   ```bash
   ls -la /usr/lib/x86_64-linux-gnu/libEGL*
   ```

3. 在远程服务器上，可能需要设置 `DISPLAY`：
   ```bash
   export DISPLAY=:0
   ```

### OpenGL 版本过低

确保 GPU 支持 OpenGL 4.3+：
```bash
glxinfo | grep "OpenGL version"
```

### 内存不足

对于大图像，可以减小输出分辨率：
```bash
./rgbd_rerender ... --W_out 640 --H_out 480
```

## 许可证

MIT License

## 致谢

- OpenCV
- OpenGL/EGL
- GLAD
