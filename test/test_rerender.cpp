/**
 * Test RGBD Rerendering Pipeline
 * 
 * This test verifies the core functionality of the rerendering pipeline.
 */

#include "types.hpp"
#include "image_io.hpp"
#include "depth_io.hpp"
#include "mesh_generator.hpp"
#include "depth_mesh.hpp"
#include "gl_renderer.hpp"

#include <iostream>
#include <cmath>
#include <cassert>
#include <filesystem>

namespace fs = std::filesystem;

// Test helper macros
#define TEST_ASSERT(cond, msg) \
    if (!(cond)) { \
        std::cerr << "FAILED: " << msg << std::endl; \
        return false; \
    } else { \
        std::cout << "PASSED: " << msg << std::endl; \
    }

/**
 * Generate synthetic RGBD data for testing
 */
void generateTestData(cv::Mat& rgb, cv::Mat& depth, int width, int height) {
    rgb.create(height, width, CV_8UC3);
    depth.create(height, width, CV_32F);
    
    // Create a gradient RGB image
    for (int v = 0; v < height; ++v) {
        for (int u = 0; u < width; ++u) {
            // Gradient from red to blue
            float t = static_cast<float>(u) / width;
            rgb.at<cv::Vec3b>(v, u) = cv::Vec3b(
                static_cast<uint8_t>(255 * (1 - t)),  // B
                static_cast<uint8_t>(128),             // G
                static_cast<uint8_t>(255 * t)          // R
            );
        }
    }
    
    // Create synthetic depth with:
    // - Background plane at 5m
    // - Foreground object (circle) at 2m
    float bgDepth = 5.0f;
    float fgDepth = 2.0f;
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    float radius = std::min(width, height) / 4.0f;
    
    for (int v = 0; v < height; ++v) {
        for (int u = 0; u < width; ++u) {
            float dx = u - cx;
            float dy = v - cy;
            float dist = std::sqrt(dx * dx + dy * dy);
            
            if (dist < radius) {
                // Foreground sphere-like object
                float z_offset = std::sqrt(radius * radius - dist * dist) / radius;
                depth.at<float>(v, u) = fgDepth - z_offset * 0.5f;
            } else {
                // Background
                depth.at<float>(v, u) = bgDepth;
            }
        }
    }
}

/**
 * Test depth discontinuity detection
 */
bool testDepthThresholds() {
    std::cout << "\n=== Testing Depth Thresholds ===" << std::endl;
    
    rgbd::DepthThresholds thresh(0.05f, 0.1f);
    
    // Same depth should not be discontinuity
    TEST_ASSERT(!thresh.isDiscontinuity(2.0f, 2.0f), 
                "Same depth is not discontinuity");
    
    // Small difference should not be discontinuity
    TEST_ASSERT(!thresh.isDiscontinuity(2.0f, 2.05f), 
                "Small difference is not discontinuity");
    
    // Large relative difference should be discontinuity
    TEST_ASSERT(thresh.isDiscontinuity(2.0f, 2.5f), 
                "Large relative difference is discontinuity");
    
    // Large absolute difference should be discontinuity
    TEST_ASSERT(thresh.isDiscontinuity(10.0f, 10.2f), 
                "Large absolute difference is discontinuity");
    
    // Invalid depths should be discontinuity
    TEST_ASSERT(thresh.isDiscontinuity(0.0f, 2.0f), 
                "Zero depth is discontinuity");
    TEST_ASSERT(thresh.isDiscontinuity(std::nanf(""), 2.0f), 
                "NaN depth is discontinuity");
    
    return true;
}

/**
 * Test mesh generation
 */
bool testMeshGeneration() {
    std::cout << "\n=== Testing Mesh Generation ===" << std::endl;
    
    cv::Mat rgb, depth;
    generateTestData(rgb, depth, 64, 64);
    
    rgbd::Intrinsics K(50.0f, 50.0f, 32.0f, 32.0f, 64, 64);
    rgbd::DepthThresholds thresh(0.05f, 0.1f);
    
    rgbd::mesh::MeshGenerator generator;
    generator.setThresholds(thresh);
    
    rgbd::Mesh mesh = generator.generate(depth, K);
    
    TEST_ASSERT(!mesh.empty(), "Mesh is not empty");
    TEST_ASSERT(mesh.numVertices() > 0, "Mesh has vertices");
    TEST_ASSERT(mesh.numTriangles() > 0, "Mesh has triangles");
    
    // Check that vertices have valid depth
    for (const auto& v : mesh.vertices) {
        TEST_ASSERT(v.z > 0, "Vertex depth is positive");
        TEST_ASSERT(std::isfinite(v.z), "Vertex depth is finite");
    }
    
    // Check that triangles are valid
    for (const auto& t : mesh.triangles) {
        TEST_ASSERT(t.v0 < mesh.numVertices(), "Triangle index v0 valid");
        TEST_ASSERT(t.v1 < mesh.numVertices(), "Triangle index v1 valid");
        TEST_ASSERT(t.v2 < mesh.numVertices(), "Triangle index v2 valid");
    }
    
    std::cout << "  Generated " << mesh.numVertices() << " vertices, "
              << mesh.numTriangles() << " triangles" << std::endl;
    
    return true;
}

/**
 * Test depth mesh builder
 */
bool testDepthMesh() {
    std::cout << "\n=== Testing Depth Mesh ===" << std::endl;
    
    cv::Mat rgb, depth;
    generateTestData(rgb, depth, 128, 128);
    
    rgbd::Intrinsics K(100.0f, 100.0f, 64.0f, 64.0f, 128, 128);
    
    rgbd::mesh::DepthMesh depthMesh;
    bool success = depthMesh.build(rgb, depth, K);
    
    TEST_ASSERT(success, "DepthMesh build succeeded");
    TEST_ASSERT(depthMesh.isValid(), "DepthMesh is valid");
    
    size_t numVerts, numTris;
    float minZ, maxZ;
    depthMesh.getStats(numVerts, numTris, minZ, maxZ);
    
    TEST_ASSERT(numVerts > 0, "Has vertices");
    TEST_ASSERT(numTris > 0, "Has triangles");
    TEST_ASSERT(minZ > 0, "Min depth is positive");
    TEST_ASSERT(maxZ > minZ, "Max depth > min depth");
    
    std::cout << "  Depth range: [" << minZ << ", " << maxZ << "]" << std::endl;
    
    return true;
}

/**
 * Test EGL/OpenGL rendering (requires GPU)
 */
bool testRenderer() {
    std::cout << "\n=== Testing Renderer ===" << std::endl;
    
    // Generate test data
    cv::Mat rgb, depth;
    generateTestData(rgb, depth, 256, 256);
    
    rgbd::Intrinsics K(200.0f, 200.0f, 128.0f, 128.0f, 256, 256);
    
    // Build mesh
    rgbd::mesh::DepthMesh depthMesh;
    if (!depthMesh.build(rgb, depth, K)) {
        std::cerr << "SKIPPED: Failed to build mesh" << std::endl;
        return true;
    }
    
    // Initialize renderer
    rgbd::render::GLRenderer renderer;
    if (!renderer.initialize()) {
        std::cerr << "SKIPPED: Failed to initialize renderer (no GPU?)" << std::endl;
        return true;  // Not a failure if no GPU
    }
    
    std::cout << renderer.getGLInfo() << std::endl;
    
    // Upload data
    TEST_ASSERT(renderer.uploadMesh(depthMesh.getMesh()), "Mesh uploaded");
    TEST_ASSERT(renderer.uploadTexture(depthMesh.getTexture()), "Texture uploaded");
    
    // Test rendering at different scales
    float scales[] = {0.5f, 1.0f, 2.0f};
    
    for (float scale : scales) {
        std::cout << "\n  Testing scale " << scale << "..." << std::endl;
        
        rgbd::Intrinsics targetK = K.scaled(scale);
        rgbd::RenderOutput output;
        
        bool renderOk = renderer.render(K, targetK, 0.1f, 100.0f, output);
        TEST_ASSERT(renderOk, "Render succeeded");
        TEST_ASSERT(output.width == K.width, "Output width correct");
        TEST_ASSERT(output.height == K.height, "Output height correct");
        
        // Check that some pixels are valid
        int validCount = 0;
        for (uint8_t m : output.mask) {
            if (m > 0) validCount++;
        }
        
        TEST_ASSERT(validCount > 0, "Has valid pixels");
        std::cout << "    Valid pixels: " << validCount << " ("
                  << (100.0f * validCount / (output.width * output.height)) << "%)" << std::endl;
        
        // Check depth values are reasonable
        float minRenderedDepth = 1e10f;
        float maxRenderedDepth = 0.0f;
        for (size_t i = 0; i < output.depth.size(); ++i) {
            if (output.mask[i] > 0) {
                minRenderedDepth = std::min(minRenderedDepth, output.depth[i]);
                maxRenderedDepth = std::max(maxRenderedDepth, output.depth[i]);
            }
        }
        
        TEST_ASSERT(minRenderedDepth > 0, "Min rendered depth is positive");
        TEST_ASSERT(maxRenderedDepth < 100.0f, "Max rendered depth is reasonable");
        std::cout << "    Rendered depth range: [" << minRenderedDepth 
                  << ", " << maxRenderedDepth << "]" << std::endl;
    }
    
    renderer.cleanup();
    return true;
}

/**
 * Test IO functions
 */
bool testIO() {
    std::cout << "\n=== Testing IO ===" << std::endl;
    
    // Create test directory
    fs::create_directories("test_output");
    
    // Generate test data
    cv::Mat rgb, depth;
    generateTestData(rgb, depth, 64, 64);
    
    // Test RGB save/load
    std::string rgbPath = "test_output/test_rgb.png";
    TEST_ASSERT(rgbd::io::saveRGB(rgbPath, rgb), "RGB save succeeded");
    
    cv::Mat loadedRgb = rgbd::io::loadRGB(rgbPath);
    TEST_ASSERT(!loadedRgb.empty(), "RGB load succeeded");
    TEST_ASSERT(loadedRgb.cols == rgb.cols, "RGB width matches");
    TEST_ASSERT(loadedRgb.rows == rgb.rows, "RGB height matches");
    
    // Test depth save/load (PNG)
    std::string depthPngPath = "test_output/test_depth.png";
    std::vector<float> depthVec(depth.begin<float>(), depth.end<float>());
    TEST_ASSERT(rgbd::io::saveDepthPNG(depthPngPath, depthVec, 64, 64, 1000.0f), 
                "Depth PNG save succeeded");
    
    // Test depth save (NPY)
    std::string depthNpyPath = "test_output/test_depth.npy";
    TEST_ASSERT(rgbd::io::saveDepthNPY(depthNpyPath, depthVec, 64, 64),
                "Depth NPY save succeeded");
    
    cv::Mat loadedDepth = rgbd::io::loadDepthNPY(depthNpyPath);
    TEST_ASSERT(!loadedDepth.empty(), "Depth NPY load succeeded");
    TEST_ASSERT(loadedDepth.cols == 64, "Depth width matches");
    TEST_ASSERT(loadedDepth.rows == 64, "Depth height matches");
    
    // Test mask save
    std::vector<uint8_t> mask(64 * 64, 1);
    std::string maskPath = "test_output/test_mask.png";
    TEST_ASSERT(rgbd::io::saveMask(maskPath, mask, 64, 64), "Mask save succeeded");
    
    // Cleanup
    fs::remove_all("test_output");
    
    return true;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "  RGBD Rerendering Test Suite          " << std::endl;
    std::cout << "========================================" << std::endl;
    
    int passed = 0;
    int total = 0;
    
    auto runTest = [&](bool (*testFunc)(), const char* name) {
        total++;
        std::cout << "\n[Test " << total << "] " << name << std::endl;
        if (testFunc()) {
            passed++;
            std::cout << ">>> Test PASSED <<<" << std::endl;
        } else {
            std::cout << ">>> Test FAILED <<<" << std::endl;
        }
    };
    
    runTest(testDepthThresholds, "Depth Thresholds");
    runTest(testMeshGeneration, "Mesh Generation");
    runTest(testDepthMesh, "Depth Mesh");
    runTest(testIO, "IO Functions");
    runTest(testRenderer, "OpenGL Renderer");
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  Results: " << passed << "/" << total << " tests passed" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return (passed == total) ? 0 : 1;
}
