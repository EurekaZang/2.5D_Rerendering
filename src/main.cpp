/**
 * RGBD Rerendering - Main Application
 * 
 * Re-renders RGBD images with different focal lengths from the same viewpoint.
 * Uses GPU-accelerated mesh rasterization via OpenGL/EGL.
 */

#include "config.hpp"
#include "image_io.hpp"
#include "depth_io.hpp"
#include "depth_mesh.hpp"
#include "gl_renderer.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    std::cout << "================================================" << std::endl;
    std::cout << "  RGBD Rerendering with Variable Focal Lengths  " << std::endl;
    std::cout << "================================================\n" << std::endl;
    
    // Parse command line arguments
    rgbd::app::Config config;
    if (!rgbd::app::parseArgs(argc, argv, config)) {
        return 1;
    }
    
    // Validate configuration
    std::string error = config.validate();
    if (!error.empty()) {
        std::cerr << "Error: " << error << std::endl;
        rgbd::app::printUsage(argv[0]);
        return 1;
    }
    
    config.print();
    
    // Create output directory
    if (!fs::exists(config.outputDir)) {
        fs::create_directories(config.outputDir);
        std::cout << "Created output directory: " << config.outputDir << std::endl;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    // Load RGB image
    std::cout << "\n[1/5] Loading RGB image..." << std::endl;
    cv::Mat rgb = rgbd::io::loadRGB(config.rgbPath);
    if (rgb.empty()) {
        std::cerr << "Error: Failed to load RGB image" << std::endl;
        return 1;
    }
    std::cout << "  Size: " << rgb.cols << "x" << rgb.rows << std::endl;
    
    // Load depth map
    std::cout << "\n[2/5] Loading depth map..." << std::endl;
    cv::Mat depth = rgbd::io::loadDepth(config.depthPath, config.depthScale);
    if (depth.empty()) {
        std::cerr << "Error: Failed to load depth map" << std::endl;
        return 1;
    }
    std::cout << "  Size: " << depth.cols << "x" << depth.rows << std::endl;
    
    // Check dimensions match
    if (rgb.cols != depth.cols || rgb.rows != depth.rows) {
        std::cerr << "Error: RGB and depth dimensions mismatch" << std::endl;
        return 1;
    }
    
    // Setup intrinsics
    rgbd::Intrinsics sourceK;
    sourceK.fx = config.fx;
    sourceK.fy = config.fy;
    sourceK.cx = (config.cx >= 0) ? config.cx : static_cast<float>(rgb.cols) / 2.0f;
    sourceK.cy = (config.cy >= 0) ? config.cy : static_cast<float>(rgb.rows) / 2.0f;
    sourceK.width = rgb.cols;
    sourceK.height = rgb.rows;
    
    std::cout << "  Intrinsics: fx=" << sourceK.fx << ", fy=" << sourceK.fy
              << ", cx=" << sourceK.cx << ", cy=" << sourceK.cy << std::endl;
    
    // Build mesh
    std::cout << "\n[3/5] Building mesh from depth..." << std::endl;
    rgbd::mesh::DepthMesh depthMesh;
    if (!depthMesh.build(rgb, depth, sourceK, config.getThresholds())) {
        std::cerr << "Error: Failed to build mesh" << std::endl;
        return 1;
    }
    
    size_t numVerts, numTris;
    float minZ, maxZ;
    depthMesh.getStats(numVerts, numTris, minZ, maxZ);
    std::cout << "  Vertices: " << numVerts << std::endl;
    std::cout << "  Triangles: " << numTris << std::endl;
    std::cout << "  Depth range: [" << minZ << ", " << maxZ << "] m" << std::endl;
    
    // Initialize renderer
    std::cout << "\n[4/5] Initializing renderer..." << std::endl;
    rgbd::render::GLRenderer renderer;
    if (!renderer.initialize(config.gpuDevice)) {
        std::cerr << "Error: Failed to initialize renderer" << std::endl;
        return 1;
    }
    std::cout << renderer.getGLInfo() << std::endl;
    
    // Upload mesh and texture
    if (!renderer.uploadMesh(depthMesh.getMesh())) {
        std::cerr << "Error: Failed to upload mesh" << std::endl;
        return 1;
    }
    
    if (!renderer.uploadTexture(depthMesh.getTexture())) {
        std::cerr << "Error: Failed to upload texture" << std::endl;
        return 1;
    }
    
    // Render with different focal lengths
    std::cout << "\n[5/5] Rendering with different focal lengths..." << std::endl;
    
    int outputW = (config.outputWidth > 0) ? config.outputWidth : sourceK.width;
    int outputH = (config.outputHeight > 0) ? config.outputHeight : sourceK.height;
    
    for (size_t i = 0; i < config.focalScales.size(); ++i) {
        float scale = config.focalScales[i];
        
        std::cout << "\n  Processing scale " << scale << " (" << (i + 1) 
                  << "/" << config.focalScales.size() << ")..." << std::endl;
        
        // Create target intrinsics
        rgbd::Intrinsics targetK = sourceK;
        targetK.fx = sourceK.fx * scale;
        targetK.fy = sourceK.fy * scale;
        targetK.width = outputW;
        targetK.height = outputH;
        
        // Adjust principal point for resolution change
        if (outputW != sourceK.width || outputH != sourceK.height) {
            targetK.cx = sourceK.cx * outputW / sourceK.width;
            targetK.cy = sourceK.cy * outputH / sourceK.height;
        }
        
        std::cout << "    Target: fx=" << targetK.fx << ", fy=" << targetK.fy
                  << ", size=" << targetK.width << "x" << targetK.height << std::endl;
        
        // Render
        rgbd::RenderOutput output;
        if (!renderer.render(sourceK, targetK, config.nearPlane, config.farPlane, output)) {
            std::cerr << "    Error: Rendering failed" << std::endl;
            continue;
        }
        
        // Generate output filenames
        std::ostringstream prefix;
        prefix << std::fixed << std::setprecision(2) << "scale_" << scale;
        std::string baseName = prefix.str();
        
        // Save RGB
        std::string rgbPath = config.outputDir + "/" + baseName + "_rgb.png";
        if (!rgbd::io::saveRGB(rgbPath, output.rgb, output.width, output.height)) {
            std::cerr << "    Warning: Failed to save RGB" << std::endl;
        } else {
            std::cout << "    Saved: " << rgbPath << std::endl;
        }
        
        // Save depth (EXR)
        if (config.saveExr) {
            std::string depthPath = config.outputDir + "/" + baseName + "_depth.exr";
            if (!rgbd::io::saveDepthEXR(depthPath, output.depth, output.width, output.height)) {
                // Fallback to TIFF if EXR not available
                std::string tiffPath = config.outputDir + "/" + baseName + "_depth.tiff";
                std::cout << "    Warning: EXR not available, saving as TIFF" << std::endl;
            } else {
                std::cout << "    Saved: " << depthPath << std::endl;
            }
        }
        
        // Save depth (PNG - 16-bit)
        if (config.savePng) {
            std::string depthPngPath = config.outputDir + "/" + baseName + "_depth.png";
            if (!rgbd::io::saveDepthPNG(depthPngPath, output.depth, 
                                        output.width, output.height, 1000.0f)) {
                std::cerr << "    Warning: Failed to save depth PNG" << std::endl;
            } else {
                std::cout << "    Saved: " << depthPngPath << std::endl;
            }
        }
        
        // Save depth (NPY)
        if (config.saveNpy) {
            std::string npyPath = config.outputDir + "/" + baseName + "_depth.npy";
            if (!rgbd::io::saveDepthNPY(npyPath, output.depth, output.width, output.height)) {
                std::cerr << "    Warning: Failed to save depth NPY" << std::endl;
            } else {
                std::cout << "    Saved: " << npyPath << std::endl;
            }
        }
        
        // Save mask
        std::string maskPath = config.outputDir + "/" + baseName + "_mask.png";
        if (!rgbd::io::saveMask(maskPath, output.mask, output.width, output.height)) {
            std::cerr << "    Warning: Failed to save mask" << std::endl;
        } else {
            std::cout << "    Saved: " << maskPath << std::endl;
        }
    }
    
    // Cleanup
    renderer.cleanup();
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
    
    std::cout << "\n================================================" << std::endl;
    std::cout << "  Done! Processed " << config.focalScales.size() << " focal scales" << std::endl;
    std::cout << "  Total time: " << duration.count() / 1000.0 << " seconds" << std::endl;
    std::cout << "  Output: " << config.outputDir << std::endl;
    std::cout << "================================================" << std::endl;
    
    return 0;
}
