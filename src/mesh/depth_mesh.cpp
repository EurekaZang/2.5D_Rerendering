#include "depth_mesh.hpp"
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <limits>
#include <cmath>

namespace rgbd {
namespace mesh {

DepthMesh::DepthMesh() {}

bool DepthMesh::build(const cv::Mat& rgb, const cv::Mat& depth,
                      const Intrinsics& intrinsics,
                      const DepthThresholds& thresholds) {
    clear();
    
    if (rgb.empty() || depth.empty()) {
        std::cerr << "Error: Empty input images" << std::endl;
        return false;
    }
    
    // Validate dimensions
    if (rgb.cols != depth.cols || rgb.rows != depth.rows) {
        std::cerr << "Error: RGB and depth dimensions mismatch" << std::endl;
        std::cerr << "  RGB: " << rgb.cols << "x" << rgb.rows << std::endl;
        std::cerr << "  Depth: " << depth.cols << "x" << depth.rows << std::endl;
        return false;
    }
    
    // Store texture (ensure it's RGB format for OpenGL)
    if (rgb.channels() == 3) {
        texture_ = rgb.clone();
    } else if (rgb.channels() == 4) {
        cv::cvtColor(rgb, texture_, cv::COLOR_BGRA2BGR);
    } else {
        cv::cvtColor(rgb, texture_, cv::COLOR_GRAY2BGR);
    }
    
    // Store intrinsics (update with actual image size)
    intrinsics_ = intrinsics;
    intrinsics_.width = depth.cols;
    intrinsics_.height = depth.rows;
    
    // Generate mesh
    MeshGenerator generator;
    generator.setThresholds(thresholds);
    mesh_ = generator.generate(depth, intrinsics_);
    
    if (mesh_.empty()) {
        std::cerr << "Error: Failed to generate mesh" << std::endl;
        return false;
    }
    
    // Compute depth statistics
    minDepth_ = std::numeric_limits<float>::max();
    maxDepth_ = 0.0f;
    
    for (const auto& vertex : mesh_.vertices) {
        if (vertex.z > 0) {
            minDepth_ = std::min(minDepth_, vertex.z);
            maxDepth_ = std::max(maxDepth_, vertex.z);
        }
    }
    
    std::cout << "Mesh built successfully" << std::endl;
    std::cout << "  Depth range: [" << minDepth_ << ", " << maxDepth_ << "] m" << std::endl;
    
    return true;
}

void DepthMesh::getStats(size_t& numVertices, size_t& numTriangles,
                         float& minDepth, float& maxDepth) const {
    numVertices = mesh_.vertices.size();
    numTriangles = mesh_.triangles.size();
    minDepth = minDepth_;
    maxDepth = maxDepth_;
}

void DepthMesh::clear() {
    mesh_.clear();
    texture_.release();
    intrinsics_ = Intrinsics();
    minDepth_ = 0.0f;
    maxDepth_ = 0.0f;
}

} // namespace mesh
} // namespace rgbd
