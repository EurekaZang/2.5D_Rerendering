#include "framebuffer.hpp"
#include <glad/glad.h>
#include <iostream>
#include <cmath>

namespace rgbd {
namespace render {

Framebuffer::Framebuffer() {}

Framebuffer::~Framebuffer() {
    destroy();
}

Framebuffer::Framebuffer(Framebuffer&& other) noexcept
    : fboId_(other.fboId_)
    , depthRbo_(other.depthRbo_)
    , width_(other.width_)
    , height_(other.height_) {
    for (int i = 0; i < 3; ++i) {
        colorTextures_[i] = other.colorTextures_[i];
        other.colorTextures_[i] = 0;
    }
    other.fboId_ = 0;
    other.depthRbo_ = 0;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& other) noexcept {
    if (this != &other) {
        destroy();
        fboId_ = other.fboId_;
        depthRbo_ = other.depthRbo_;
        width_ = other.width_;
        height_ = other.height_;
        for (int i = 0; i < 3; ++i) {
            colorTextures_[i] = other.colorTextures_[i];
            other.colorTextures_[i] = 0;
        }
        other.fboId_ = 0;
        other.depthRbo_ = 0;
    }
    return *this;
}

bool Framebuffer::create(int width, int height) {
    destroy();
    
    width_ = width;
    height_ = height;
    
    // Create framebuffer
    glGenFramebuffers(1, &fboId_);
    glBindFramebuffer(GL_FRAMEBUFFER, fboId_);
    
    // Create color texture 0: RGB output (RGBA8)
    glGenTextures(1, &colorTextures_[0]);
    glBindTexture(GL_TEXTURE_2D, colorTextures_[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTextures_[0], 0);
    
    // Create color texture 1: Metric depth (R32F)
    glGenTextures(1, &colorTextures_[1]);
    glBindTexture(GL_TEXTURE_2D, colorTextures_[1]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, colorTextures_[1], 0);
    
    // Create color texture 2: Mask (R8)
    glGenTextures(1, &colorTextures_[2]);
    glBindTexture(GL_TEXTURE_2D, colorTextures_[2]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, colorTextures_[2], 0);
    
    // Create depth renderbuffer for z-test
    glGenRenderbuffers(1, &depthRbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo_);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo_);
    
    // Set draw buffers
    GLenum drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };
    glDrawBuffers(3, drawBuffers);
    
    // Check framebuffer completeness
    if (!checkStatus()) {
        destroy();
        return false;
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

void Framebuffer::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fboId_);
    glViewport(0, 0, width_, height_);
}

void Framebuffer::unbind() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::clear(float clearDepthValue) const {
    bind();
    
    // Clear color buffer to black
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Clear depth buffer
    glClearDepthf(1.0f);
    
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // Clear metric depth texture to clearDepthValue
    // We'll write the clear value via a simple fullscreen quad or just let the rendering handle it
    // For now, we rely on the fragment shader to write proper values
}

void Framebuffer::readRGB(std::vector<uint8_t>& data) const {
    data.resize(width_ * height_ * 3);
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId_);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    
    // Read as RGBA first, then convert to RGB
    std::vector<uint8_t> rgba(width_ * height_ * 4);
    glReadPixels(0, 0, width_, height_, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    
    // Convert RGBA to RGB and flip vertically (OpenGL has origin at bottom-left)
    for (int y = 0; y < height_; ++y) {
        int srcY = height_ - 1 - y;  // Flip Y
        for (int x = 0; x < width_; ++x) {
            int srcIdx = (srcY * width_ + x) * 4;
            int dstIdx = (y * width_ + x) * 3;
            data[dstIdx + 0] = rgba[srcIdx + 0];  // R
            data[dstIdx + 1] = rgba[srcIdx + 1];  // G
            data[dstIdx + 2] = rgba[srcIdx + 2];  // B
        }
    }
}

void Framebuffer::readDepth(std::vector<float>& data) const {
    data.resize(width_ * height_);
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId_);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    
    std::vector<float> raw(width_ * height_);
    glReadPixels(0, 0, width_, height_, GL_RED, GL_FLOAT, raw.data());
    
    // Flip vertically
    for (int y = 0; y < height_; ++y) {
        int srcY = height_ - 1 - y;
        for (int x = 0; x < width_; ++x) {
            data[y * width_ + x] = raw[srcY * width_ + x];
        }
    }
}

void Framebuffer::readMask(std::vector<uint8_t>& data) const {
    data.resize(width_ * height_);
    
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboId_);
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    
    std::vector<uint8_t> raw(width_ * height_);
    glReadPixels(0, 0, width_, height_, GL_RED, GL_UNSIGNED_BYTE, raw.data());
    
    // Flip vertically
    for (int y = 0; y < height_; ++y) {
        int srcY = height_ - 1 - y;
        for (int x = 0; x < width_; ++x) {
            data[y * width_ + x] = raw[srcY * width_ + x];
        }
    }
}

void Framebuffer::destroy() {
    if (fboId_ != 0) {
        glDeleteFramebuffers(1, &fboId_);
        fboId_ = 0;
    }
    
    for (int i = 0; i < 3; ++i) {
        if (colorTextures_[i] != 0) {
            glDeleteTextures(1, &colorTextures_[i]);
            colorTextures_[i] = 0;
        }
    }
    
    if (depthRbo_ != 0) {
        glDeleteRenderbuffers(1, &depthRbo_);
        depthRbo_ = 0;
    }
    
    width_ = 0;
    height_ = 0;
}

bool Framebuffer::checkStatus() const {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    
    if (status == GL_FRAMEBUFFER_COMPLETE) {
        return true;
    }
    
    std::cerr << "Framebuffer incomplete: ";
    switch (status) {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            std::cerr << "Incomplete attachment" << std::endl;
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            std::cerr << "Missing attachment" << std::endl;
            break;
        default:
            std::cerr << "Unknown error: " << status << std::endl;
            break;
    }
    
    return false;
}

} // namespace render
} // namespace rgbd
