#pragma once

#include "types.hpp"
#include "depth_mesh.hpp"
#include "egl_context.hpp"
#include "shader.hpp"
#include "framebuffer.hpp"
#include <opencv2/core.hpp>
#include <memory>

namespace rgbd {
namespace render {

/**
 * OpenGL renderer for RGBD re-rendering
 * 
 * This class handles:
 * - Uploading mesh data to GPU (VBO/EBO)
 * - Uploading RGB texture
 * - Setting up projection matrix from intrinsics
 * - Rendering to FBO with MRT (RGB, depth, mask)
 * - Reading back results
 */
class GLRenderer {
public:
    GLRenderer();
    ~GLRenderer();
    
    // Non-copyable
    GLRenderer(const GLRenderer&) = delete;
    GLRenderer& operator=(const GLRenderer&) = delete;
    
    /**
     * Initialize renderer (EGL context, shaders, etc.)
     * @param gpuDevice GPU device index (-1 for default)
     * @return true on success
     */
    bool initialize(int gpuDevice = -1);
    
    /**
     * Upload mesh data to GPU
     * @param mesh Mesh with vertices and triangles
     * @return true on success
     */
    bool uploadMesh(const Mesh& mesh);
    
    /**
     * Upload RGB texture to GPU
     * @param texture RGB image (CV_8UC3)
     * @return true on success
     */
    bool uploadTexture(const cv::Mat& texture);
    
    /**
     * Render with target intrinsics
     * @param sourceK Source camera intrinsics (used for mesh creation)
     * @param targetK Target camera intrinsics (for rendering)
     * @param nearPlane Near clipping plane (meters)
     * @param farPlane Far clipping plane (meters)
     * @param output Output render targets
     * @return true on success
     */
    bool render(const Intrinsics& sourceK, const Intrinsics& targetK,
                float nearPlane, float farPlane, RenderOutput& output);
    
    /**
     * Check if renderer is initialized
     */
    bool isInitialized() const { return initialized_; }
    
    /**
     * Get OpenGL info
     */
    std::string getGLInfo() const;
    
    /**
     * Cleanup all resources
     */
    void cleanup();
    
private:
    GLContext eglContext_;
    Shader shader_;
    Framebuffer framebuffer_;
    
    // OpenGL resources
    uint32_t vao_ = 0;
    uint32_t vbo_ = 0;
    uint32_t ebo_ = 0;
    uint32_t rgbTexture_ = 0;
    size_t numIndices_ = 0;
    
    bool initialized_ = false;
    
    /**
     * Create OpenGL projection matrix from intrinsics
     * @param K Camera intrinsics
     * @param near Near plane
     * @param far Far plane
     * @param matrix Output 4x4 matrix (column-major)
     */
    void createProjectionMatrix(const Intrinsics& K, 
                                float near, float far,
                                float* matrix) const;
    
    /**
     * Initialize shaders
     */
    bool initShaders();
    
    /**
     * Create VAO/VBO/EBO
     */
    bool createBuffers();
    
    /**
     * Delete all GPU resources
     */
    void deleteBuffers();
};

} // namespace render
} // namespace rgbd
