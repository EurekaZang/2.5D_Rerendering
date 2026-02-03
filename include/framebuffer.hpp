#pragma once

#include <cstdint>
#include <vector>

namespace rgbd {
namespace render {

/**
 * Framebuffer object with multiple render targets (MRT)
 * 
 * Attachments:
 * - Color0: RGB output (RGBA8)
 * - Color1: Metric depth (R32F) 
 * - Color2: Validity mask (R8)
 * - Depth: Z-buffer for depth testing
 */
class Framebuffer {
public:
    Framebuffer();
    ~Framebuffer();
    
    // Non-copyable
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    
    // Movable
    Framebuffer(Framebuffer&& other) noexcept;
    Framebuffer& operator=(Framebuffer&& other) noexcept;
    
    /**
     * Create framebuffer with specified size
     * @param width Framebuffer width
     * @param height Framebuffer height
     * @return true on success
     */
    bool create(int width, int height);
    
    /**
     * Bind this framebuffer for rendering
     */
    void bind() const;
    
    /**
     * Unbind framebuffer (bind default)
     */
    static void unbind();
    
    /**
     * Clear all attachments
     * @param clearDepthValue Value for metric depth buffer (default: 0)
     */
    void clear(float clearDepthValue = 0.0f) const;
    
    /**
     * Read RGB data from framebuffer
     * @param data Output buffer (must be preallocated: width * height * 3)
     */
    void readRGB(std::vector<uint8_t>& data) const;
    
    /**
     * Read metric depth from framebuffer
     * @param data Output buffer (must be preallocated: width * height)
     */
    void readDepth(std::vector<float>& data) const;
    
    /**
     * Read mask from framebuffer
     * @param data Output buffer (must be preallocated: width * height)
     */
    void readMask(std::vector<uint8_t>& data) const;
    
    /**
     * Get framebuffer dimensions
     */
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }
    
    /**
     * Check if framebuffer is valid
     */
    bool isValid() const { return fboId_ != 0; }
    
    /**
     * Get texture IDs
     */
    uint32_t getRGBTexture() const { return colorTextures_[0]; }
    uint32_t getDepthTexture() const { return colorTextures_[1]; }
    uint32_t getMaskTexture() const { return colorTextures_[2]; }
    
    /**
     * Destroy framebuffer
     */
    void destroy();
    
private:
    uint32_t fboId_ = 0;
    uint32_t colorTextures_[3] = {0, 0, 0};  // RGB, Depth, Mask
    uint32_t depthRbo_ = 0;  // Renderbuffer for z-test
    int width_ = 0;
    int height_ = 0;
    
    /**
     * Check framebuffer completeness
     */
    bool checkStatus() const;
};

} // namespace render
} // namespace rgbd
