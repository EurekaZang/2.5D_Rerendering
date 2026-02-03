#include "mesh_generator.hpp"
#include <cmath>
#include <iostream>

namespace rgbd {
namespace mesh {

MeshGenerator::MeshGenerator() : thresholds_() {}

void MeshGenerator::setThresholds(const DepthThresholds& thresholds) {
    thresholds_ = thresholds;
}

Vertex MeshGenerator::backproject(float u, float v, float z, const Intrinsics& K) {
    // Back-project pixel center to 3D camera space
    // The pixel (u, v) covers the area [u, u+1) x [v, v+1)
    // Its center is at (u + 0.5, v + 0.5)
    //
    // Using pixel center for both 3D position and texture coordinates
    // ensures consistency when rendering back at the same focal length
    float u_center = u + 0.5f;
    float v_center = v + 0.5f;
    
    // Back-project using pixel center
    // X = (u_center - cx) * z / fx
    // Y = (v_center - cy) * z / fy
    // Z = z
    float X = (u_center - K.cx) * z / K.fx;
    float Y = (v_center - K.cy) * z / K.fy;
    
    // Compute texture coordinates (normalized) - also using pixel center
    float tex_u = u_center / static_cast<float>(K.width);
    float tex_v = v_center / static_cast<float>(K.height);
    
    return Vertex(X, Y, z, tex_u, tex_v);
}

bool MeshGenerator::shouldBreakEdge(float z1, float z2) const {
    return thresholds_.isDiscontinuity(z1, z2);
}

bool MeshGenerator::isValidTriangle(float z0, float z1, float z2) const {
    // Check if all depths are valid
    if (!isValidDepth(z0) || !isValidDepth(z1) || !isValidDepth(z2)) {
        return false;
    }
    
    // Check all three edges for discontinuities
    if (shouldBreakEdge(z0, z1)) return false;
    if (shouldBreakEdge(z1, z2)) return false;
    if (shouldBreakEdge(z2, z0)) return false;
    
    return true;
}

Mesh MeshGenerator::generate(const cv::Mat& depth, const Intrinsics& intrinsics) {
    cv::Mat validMask;
    return generate(depth, intrinsics, validMask);
}

Mesh MeshGenerator::generate(const cv::Mat& depth, const Intrinsics& intrinsics,
                             const cv::Mat& validMask) {
    Mesh mesh;
    
    if (depth.empty()) {
        std::cerr << "Error: Empty depth map" << std::endl;
        return mesh;
    }
    
    int H = depth.rows;
    int W = depth.cols;
    
    // Ensure depth is float32
    cv::Mat depthF;
    if (depth.type() != CV_32F) {
        depth.convertTo(depthF, CV_32F);
    } else {
        depthF = depth;
    }
    
    // Create vertex grid (store vertex index for each pixel, -1 if invalid)
    std::vector<int> vertexGrid(W * H, -1);
    
    // Generate vertices for all valid depth pixels
    mesh.vertices.reserve(W * H);
    
    for (int v = 0; v < H; ++v) {
        const float* depthRow = depthF.ptr<float>(v);
        const uint8_t* maskRow = validMask.empty() ? nullptr : validMask.ptr<uint8_t>(v);
        
        for (int u = 0; u < W; ++u) {
            float z = depthRow[u];
            
            // Check validity
            bool valid = isValidDepth(z);
            if (maskRow) {
                valid = valid && (maskRow[u] > 0);
            }
            
            if (valid) {
                vertexGrid[v * W + u] = static_cast<int>(mesh.vertices.size());
                mesh.vertices.push_back(backproject(static_cast<float>(u), 
                                                    static_cast<float>(v), 
                                                    z, intrinsics));
            }
        }
    }
    
    // Generate triangles from quad grid
    // For each quad (u,v), (u+1,v), (u,v+1), (u+1,v+1)
    // Create two triangles if all vertices exist and edges are valid
    mesh.triangles.reserve((W - 1) * (H - 1) * 2);
    
    for (int v = 0; v < H - 1; ++v) {
        for (int u = 0; u < W - 1; ++u) {
            // Get vertex indices for quad corners
            int idx00 = vertexGrid[v * W + u];           // Top-left
            int idx10 = vertexGrid[v * W + (u + 1)];     // Top-right
            int idx01 = vertexGrid[(v + 1) * W + u];     // Bottom-left
            int idx11 = vertexGrid[(v + 1) * W + (u + 1)]; // Bottom-right
            
            // Get depth values
            float z00 = depthF.at<float>(v, u);
            float z10 = depthF.at<float>(v, u + 1);
            float z01 = depthF.at<float>(v + 1, u);
            float z11 = depthF.at<float>(v + 1, u + 1);
            
            // Triangle 1: v00, v10, v11
            if (idx00 >= 0 && idx10 >= 0 && idx11 >= 0) {
                if (isValidTriangle(z00, z10, z11)) {
                    mesh.triangles.emplace_back(
                        static_cast<uint32_t>(idx00),
                        static_cast<uint32_t>(idx10),
                        static_cast<uint32_t>(idx11)
                    );
                }
            }
            
            // Triangle 2: v00, v11, v01
            if (idx00 >= 0 && idx11 >= 0 && idx01 >= 0) {
                if (isValidTriangle(z00, z11, z01)) {
                    mesh.triangles.emplace_back(
                        static_cast<uint32_t>(idx00),
                        static_cast<uint32_t>(idx11),
                        static_cast<uint32_t>(idx01)
                    );
                }
            }
        }
    }
    
    mesh.vertices.shrink_to_fit();
    mesh.triangles.shrink_to_fit();
    
    std::cout << "Generated mesh: " << mesh.vertices.size() << " vertices, "
              << mesh.triangles.size() << " triangles" << std::endl;
    
    return mesh;
}

} // namespace mesh
} // namespace rgbd
