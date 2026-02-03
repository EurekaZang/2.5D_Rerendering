#include "gl_renderer.hpp"
#include <glad/glad.h>
#include <iostream>
#include <cstring>
#include <cmath>

namespace rgbd {
namespace render {

// Embedded shader sources
static const char* vertexShaderSource = R"(
#version 330 core

layout(location = 0) in vec3 aPosition;  // Camera-space position (X, Y, Z)
layout(location = 1) in vec2 aTexCoord;  // Texture coordinates

uniform mat4 uProjection;

out vec2 vTexCoord;
out float vDepth;

void main() {
    // Transform to clip space using projection matrix
    gl_Position = uProjection * vec4(aPosition, 1.0);
    
    // Pass through texture coordinates and metric depth
    vTexCoord = aTexCoord;
    vDepth = aPosition.z;  // Camera-space Z is the metric depth
}
)";

static const char* fragmentShaderSource = R"(
#version 330 core

in vec2 vTexCoord;
in float vDepth;

uniform sampler2D uRGBTexture;

layout(location = 0) out vec4 outColor;     // RGB output
layout(location = 1) out float outDepth;    // Metric depth output
layout(location = 2) out float outMask;     // Validity mask output

void main() {
    // Sample RGB texture
    vec4 color = texture(uRGBTexture, vTexCoord);
    
    // Output RGB
    outColor = color;
    
    // Output metric depth (camera Z in meters)
    outDepth = vDepth;
    
    // Output mask (1.0 = valid rendered pixel)
    outMask = 1.0;
}
)";

GLRenderer::GLRenderer() {}

GLRenderer::~GLRenderer() {
    cleanup();
}

bool GLRenderer::initialize(int gpuDevice) {
    if (initialized_) {
        return true;
    }
    
    // Initialize EGL context
    if (!eglContext_.initialize(gpuDevice)) {
        std::cerr << "Error: Failed to initialize EGL context" << std::endl;
        return false;
    }
    
    // Initialize shaders
    if (!initShaders()) {
        std::cerr << "Error: Failed to initialize shaders" << std::endl;
        return false;
    }
    
    // Create buffers
    if (!createBuffers()) {
        std::cerr << "Error: Failed to create buffers" << std::endl;
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool GLRenderer::initShaders() {
    return shader_.loadFromSource(vertexShaderSource, fragmentShaderSource);
}

bool GLRenderer::createBuffers() {
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    return true;
}

void GLRenderer::deleteBuffers() {
    if (rgbTexture_ != 0) {
        glDeleteTextures(1, &rgbTexture_);
        rgbTexture_ = 0;
    }
    
    if (ebo_ != 0) {
        glDeleteBuffers(1, &ebo_);
        ebo_ = 0;
    }
    
    if (vbo_ != 0) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    
    if (vao_ != 0) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
}

bool GLRenderer::uploadMesh(const Mesh& mesh) {
    if (!initialized_) {
        std::cerr << "Error: Renderer not initialized" << std::endl;
        return false;
    }
    
    if (mesh.empty()) {
        std::cerr << "Error: Empty mesh" << std::endl;
        return false;
    }
    
    glBindVertexArray(vao_);
    
    // Upload vertex data
    // Each vertex: 3 floats position + 2 floats UV = 5 floats
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 
                 mesh.vertices.size() * sizeof(Vertex),
                 mesh.vertices.data(), 
                 GL_STATIC_DRAW);
    
    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                          reinterpret_cast<void*>(0));
    
    // Texture coordinate attribute (location 1)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), 
                          reinterpret_cast<void*>(3 * sizeof(float)));
    
    // Upload index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mesh.triangles.size() * sizeof(Triangle),
                 mesh.triangles.data(),
                 GL_STATIC_DRAW);
    
    numIndices_ = mesh.triangles.size() * 3;
    
    glBindVertexArray(0);
    
    std::cout << "Uploaded mesh: " << mesh.vertices.size() << " vertices, "
              << mesh.triangles.size() << " triangles" << std::endl;
    
    return true;
}

bool GLRenderer::uploadTexture(const cv::Mat& texture) {
    if (!initialized_) {
        std::cerr << "Error: Renderer not initialized" << std::endl;
        return false;
    }
    
    if (texture.empty()) {
        std::cerr << "Error: Empty texture" << std::endl;
        return false;
    }
    
    // Create texture if needed
    if (rgbTexture_ == 0) {
        glGenTextures(1, &rgbTexture_);
    }
    
    glBindTexture(GL_TEXTURE_2D, rgbTexture_);
    
    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // Upload texture data
    // OpenCV stores images as BGR, so we need to handle that
    GLenum format = GL_BGR;
    if (texture.channels() == 4) {
        format = GL_BGRA;
    } else if (texture.channels() == 1) {
        format = GL_RED;
    }
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 
                 texture.cols, texture.rows, 0,
                 format, GL_UNSIGNED_BYTE, texture.data);
    
    glBindTexture(GL_TEXTURE_2D, 0);
    
    std::cout << "Uploaded texture: " << texture.cols << "x" << texture.rows << std::endl;
    
    return true;
}

void GLRenderer::createProjectionMatrix(const Intrinsics& K, 
                                         float near, float far,
                                         float* matrix) const {
    // Create OpenGL projection matrix from camera intrinsics
    // Our camera space: X right, Y down, Z forward (looking into +Z)
    // OpenGL clip space: X right, Y up, Z into screen, NDC in [-1,1]
    //
    // For pinhole camera projection:
    //   u = fx * X/Z + cx
    //   v = fy * Y/Z + cy
    //
    // Converting to NDC:
    //   x_ndc = 2*u/W - 1 = 2*fx*X/(W*Z) + (2*cx/W - 1)
    //   y_ndc = 1 - 2*v/H = -2*fy*Y/(H*Z) + (1 - 2*cy/H)
    //
    // In clip space (before perspective divide by w=Z):
    //   clip_x = 2*fx/W * X + (2*cx/W - 1) * Z
    //   clip_y = -2*fy/H * Y + (1 - 2*cy/H) * Z
    //   clip_w = Z
    
    float W = static_cast<float>(K.width);
    float H = static_cast<float>(K.height);
    float n = near;
    float f = far;
    
    // Clear matrix
    std::memset(matrix, 0, 16 * sizeof(float));
    
    // Column-major order for OpenGL
    // [0]  [4]  [8]  [12]
    // [1]  [5]  [9]  [13]
    // [2]  [6]  [10] [14]
    // [3]  [7]  [11] [15]
    
    // Projection matrix mapping camera space to clip space
    matrix[0] = 2.0f * K.fx / W;                        // X scaling
    matrix[5] = -2.0f * K.fy / H;                       // Y scaling (flip Y: camera Y down -> NDC Y up)
    matrix[8] = 2.0f * K.cx / W - 1.0f;                 // cx offset in NDC
    matrix[9] = 1.0f - 2.0f * K.cy / H;                 // cy offset in NDC (with Y flip)
    matrix[10] = (f + n) / (f - n);                     // Depth mapping
    matrix[11] = 1.0f;                                   // w = Z (positive Z forward)
    matrix[14] = -2.0f * f * n / (f - n);               // Depth offset
    matrix[15] = 0.0f;
}

bool GLRenderer::render(const Intrinsics& sourceK, const Intrinsics& targetK,
                        float nearPlane, float farPlane, RenderOutput& output) {
    if (!initialized_) {
        std::cerr << "Error: Renderer not initialized" << std::endl;
        return false;
    }
    
    if (numIndices_ == 0) {
        std::cerr << "Error: No mesh uploaded" << std::endl;
        return false;
    }
    
    if (rgbTexture_ == 0) {
        std::cerr << "Error: No texture uploaded" << std::endl;
        return false;
    }
    
    int outWidth = targetK.width;
    int outHeight = targetK.height;
    
    // Create or resize framebuffer
    if (!framebuffer_.isValid() || 
        framebuffer_.getWidth() != outWidth || 
        framebuffer_.getHeight() != outHeight) {
        if (!framebuffer_.create(outWidth, outHeight)) {
            std::cerr << "Error: Failed to create framebuffer" << std::endl;
            return false;
        }
    }
    
    // Bind framebuffer
    framebuffer_.bind();
    
    // Clear
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepthf(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Enable depth testing
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    
    // Disable face culling (we want to see both sides)
    glDisable(GL_CULL_FACE);
    
    // Use shader
    shader_.use();
    
    // Set projection matrix
    float projMatrix[16];
    createProjectionMatrix(targetK, nearPlane, farPlane, projMatrix);
    shader_.setUniformMatrix4("uProjection", projMatrix);
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, rgbTexture_);
    shader_.setUniform("uRGBTexture", 0);
    
    // Draw mesh
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(numIndices_), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
    
    // Read back results
    output.allocate(outWidth, outHeight);
    framebuffer_.readRGB(output.rgb);
    framebuffer_.readDepth(output.depth);
    framebuffer_.readMask(output.mask);
    
    // Unbind
    Framebuffer::unbind();
    
    // Count valid pixels
    int validCount = 0;
    for (uint8_t m : output.mask) {
        if (m > 0) validCount++;
    }
    std::cout << "Rendered " << validCount << " valid pixels ("
              << (100.0f * validCount / (outWidth * outHeight)) << "%)" << std::endl;
    
    return true;
}

std::string GLRenderer::getGLInfo() const {
    if (!initialized_) return "Not initialized";
    return "OpenGL: " + eglContext_.getGLVersion() + "\n" +
           "Renderer: " + eglContext_.getGLRenderer();
}

void GLRenderer::cleanup() {
    framebuffer_.destroy();
    shader_.destroy();
    deleteBuffers();
    eglContext_.destroy();
    initialized_ = false;
}

} // namespace render
} // namespace rgbd
