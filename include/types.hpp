#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <limits>
#include <cmath>

namespace rgbd {

// Camera intrinsics
struct Intrinsics {
    float fx = 525.0f;  // Focal length x
    float fy = 525.0f;  // Focal length y
    float cx = 320.0f;  // Principal point x
    float cy = 240.0f;  // Principal point y
    int width = 640;    // Image width
    int height = 480;   // Image height
    
    Intrinsics() = default;
    Intrinsics(float fx_, float fy_, float cx_, float cy_, int w, int h)
        : fx(fx_), fy(fy_), cx(cx_), cy(cy_), width(w), height(h) {}
    
    // Create scaled intrinsics (for zoom)
    Intrinsics scaled(float scale) const {
        return Intrinsics(fx * scale, fy * scale, cx, cy, width, height);
    }
    
    // Create with different resolution
    Intrinsics withResolution(int w, int h) const {
        float scale_x = static_cast<float>(w) / width;
        float scale_y = static_cast<float>(h) / height;
        return Intrinsics(fx * scale_x, fy * scale_y, 
                         cx * scale_x, cy * scale_y, w, h);
    }
};

// 3D vertex with texture coordinates
struct Vertex {
    float x, y, z;     // Position in camera space
    float u, v;        // Texture coordinates
    
    Vertex() : x(0), y(0), z(0), u(0), v(0) {}
    Vertex(float x_, float y_, float z_, float u_, float v_)
        : x(x_), y(y_), z(z_), u(u_), v(v_) {}
};

// Triangle indices
struct Triangle {
    uint32_t v0, v1, v2;
    
    Triangle() : v0(0), v1(0), v2(0) {}
    Triangle(uint32_t a, uint32_t b, uint32_t c) : v0(a), v1(b), v2(c) {}
};

// Mesh data structure
struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<Triangle> triangles;
    
    void clear() {
        vertices.clear();
        triangles.clear();
    }
    
    bool empty() const {
        return vertices.empty() || triangles.empty();
    }
    
    size_t numVertices() const { return vertices.size(); }
    size_t numTriangles() const { return triangles.size(); }
};

// Rendering output
struct RenderOutput {
    std::vector<uint8_t> rgb;      // HxWx3 RGB
    std::vector<float> depth;      // HxW metric depth (meters)
    std::vector<uint8_t> mask;     // HxW validity mask
    int width = 0;
    int height = 0;
    
    void allocate(int w, int h) {
        width = w;
        height = h;
        rgb.resize(w * h * 3, 0);
        depth.resize(w * h, 0.0f);
        mask.resize(w * h, 0);
    }
    
    void clear() {
        std::fill(rgb.begin(), rgb.end(), 0);
        std::fill(depth.begin(), depth.end(), 0.0f);
        std::fill(mask.begin(), mask.end(), 0);
    }
};

// Depth discontinuity thresholds
struct DepthThresholds {
    float tau_rel = 0.05f;   // Relative threshold (5%)
    float tau_abs = 0.1f;    // Absolute threshold (10cm)
    
    DepthThresholds() = default;
    DepthThresholds(float rel, float abs) : tau_rel(rel), tau_abs(abs) {}
    
    // Check if two depth values have a discontinuity
    bool isDiscontinuity(float z1, float z2) const {
        if (!std::isfinite(z1) || !std::isfinite(z2)) return true;
        if (z1 <= 0 || z2 <= 0) return true;
        
        float diff = std::abs(z1 - z2);
        float min_z = std::min(z1, z2);
        
        // Check both relative and absolute thresholds
        return (diff / min_z > tau_rel) || (diff > tau_abs);
    }
};

// Utility functions
inline bool isValidDepth(float z) {
    return std::isfinite(z) && z > 0.0f;
}

inline float clamp(float v, float lo, float hi) {
    return (v < lo) ? lo : ((v > hi) ? hi : v);
}

} // namespace rgbd
