#pragma once

#include "types.hpp"
#include <opencv2/core.hpp>

namespace rgbd {
namespace mesh {

/**
 * Generate a 2.5D mesh from a depth map
 * 
 * This class creates a triangulated mesh from a depth map by:
 * 1. Back-projecting each valid depth pixel to 3D (camera space)
 * 2. Creating vertices with texture coordinates
 * 3. Building triangles from adjacent pixels
 * 4. Breaking edges at depth discontinuities to avoid rubber-sheet artifacts
 */
class MeshGenerator {
public:
    MeshGenerator();
    ~MeshGenerator() = default;
    
    /**
     * Set depth discontinuity thresholds
     * @param thresholds Relative and absolute thresholds
     */
    void setThresholds(const DepthThresholds& thresholds);
    
    /**
     * Generate mesh from depth map
     * @param depth Depth map (float32, meters)
     * @param intrinsics Camera intrinsics
     * @return Generated mesh
     */
    Mesh generate(const cv::Mat& depth, const Intrinsics& intrinsics);
    
    /**
     * Generate mesh with custom invalid depth handling
     * @param depth Depth map
     * @param intrinsics Camera intrinsics
     * @param validMask Pre-computed validity mask (optional)
     * @return Generated mesh
     */
    Mesh generate(const cv::Mat& depth, const Intrinsics& intrinsics,
                  const cv::Mat& validMask);
    
private:
    DepthThresholds thresholds_;
    
    /**
     * Back-project a pixel to 3D camera space
     * @param u Pixel x coordinate
     * @param v Pixel y coordinate
     * @param z Depth value (meters)
     * @param K Camera intrinsics
     * @return 3D point in camera space
     */
    Vertex backproject(float u, float v, float z, const Intrinsics& K);
    
    /**
     * Check if an edge between two depth values should be broken
     */
    bool shouldBreakEdge(float z1, float z2) const;
    
    /**
     * Check if a triangle should be created (all edges valid)
     */
    bool isValidTriangle(float z0, float z1, float z2) const;
};

} // namespace mesh
} // namespace rgbd
