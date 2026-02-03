#pragma once

#include "types.hpp"
#include "mesh_generator.hpp"
#include <opencv2/core.hpp>
#include <memory>

namespace rgbd {
namespace mesh {

/**
 * High-level depth mesh class that manages mesh generation and caching
 */
class DepthMesh {
public:
    DepthMesh();
    ~DepthMesh() = default;
    
    /**
     * Build mesh from RGBD data
     * @param rgb RGB image (for texture)
     * @param depth Depth map (meters)
     * @param intrinsics Source camera intrinsics
     * @param thresholds Depth discontinuity thresholds
     * @return true on success
     */
    bool build(const cv::Mat& rgb, const cv::Mat& depth,
               const Intrinsics& intrinsics,
               const DepthThresholds& thresholds = DepthThresholds());
    
    /**
     * Get the generated mesh
     */
    const Mesh& getMesh() const { return mesh_; }
    
    /**
     * Get the RGB texture
     */
    const cv::Mat& getTexture() const { return texture_; }
    
    /**
     * Get source intrinsics
     */
    const Intrinsics& getIntrinsics() const { return intrinsics_; }
    
    /**
     * Check if mesh is valid
     */
    bool isValid() const { return !mesh_.empty(); }
    
    /**
     * Get mesh statistics
     */
    void getStats(size_t& numVertices, size_t& numTriangles,
                  float& minDepth, float& maxDepth) const;
    
    /**
     * Clear mesh data
     */
    void clear();
    
private:
    Mesh mesh_;
    cv::Mat texture_;
    Intrinsics intrinsics_;
    float minDepth_ = 0.0f;
    float maxDepth_ = 0.0f;
};

} // namespace mesh
} // namespace rgbd
