#pragma once

#include <string>
#include <cstdint>

namespace rgbd {
namespace render {

/**
 * EGL context for headless OpenGL rendering
 * 
 * This class creates an EGL context without requiring a display,
 * enabling GPU-accelerated rendering on servers and in CLI pipelines.
 */
class GLContext {
public:
    GLContext();
    ~GLContext();
    
    // Non-copyable
    GLContext(const GLContext&) = delete;
    GLContext& operator=(const GLContext&) = delete;
    
    // Movable
    GLContext(GLContext&& other) noexcept;
    GLContext& operator=(GLContext&& other) noexcept;
    
    /**
     * Initialize EGL context
     * @param deviceIndex GPU device index (-1 for default)
     * @return true on success
     */
    bool initialize(int deviceIndex = -1);
    
    /**
     * Make this context current
     * @return true on success
     */
    bool makeCurrent();
    
    /**
     * Release current context
     */
    void releaseCurrent();
    
    /**
     * Check if context is valid
     */
    bool isValid() const { return initialized_; }
    
    /**
     * Get OpenGL version string
     */
    std::string getGLVersion() const;
    
    /**
     * Get GPU renderer string
     */
    std::string getGLRenderer() const;
    
    /**
     * Cleanup and destroy context
     */
    void destroy();
    
private:
    void* display_ = nullptr;
    void* context_ = nullptr;
    void* surface_ = nullptr;
    bool initialized_ = false;
};

} // namespace render
} // namespace rgbd
