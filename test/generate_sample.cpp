/**
 * Generate Sample RGBD Data
 * 
 * Creates synthetic RGB and depth images for testing the rerendering pipeline.
 */

#include "image_io.hpp"
#include "depth_io.hpp"

#include <opencv2/core.hpp>
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <cmath>
#include <filesystem>

namespace fs = std::filesystem;

void generateCheckerboard(cv::Mat& rgb, int width, int height, int tileSize) {
    rgb.create(height, width, CV_8UC3);
    
    for (int v = 0; v < height; ++v) {
        for (int u = 0; u < width; ++u) {
            int tileU = u / tileSize;
            int tileV = v / tileSize;
            bool isWhite = (tileU + tileV) % 2 == 0;
            
            // Add color variation based on position
            uint8_t r = isWhite ? 255 : static_cast<uint8_t>(50 + 150.0f * u / width);
            uint8_t g = isWhite ? 255 : static_cast<uint8_t>(50 + 150.0f * v / height);
            uint8_t b = isWhite ? 255 : 50;
            
            rgb.at<cv::Vec3b>(v, u) = cv::Vec3b(b, g, r);  // BGR format
        }
    }
}

void generateComplexScene(cv::Mat& rgb, cv::Mat& depth, int width, int height) {
    rgb.create(height, width, CV_8UC3);
    depth.create(height, width, CV_32F);
    
    float cx = width / 2.0f;
    float cy = height / 2.0f;
    
    // Background depth
    float bgDepth = 8.0f;
    
    for (int v = 0; v < height; ++v) {
        for (int u = 0; u < width; ++u) {
            float x = u - cx;
            float y = v - cy;
            
            // Default: background
            float z = bgDepth;
            uint8_t r = 100, g = 100, b = 150;  // Sky blue
            
            // Ground plane (bottom half)
            if (v > height / 2) {
                z = bgDepth - 2.0f * (v - height / 2.0f) / height;
                z = std::max(z, 1.5f);
                r = 80; g = 120; b = 80;  // Green grass
            }
            
            // Sphere 1: Left side
            float sphere1_cx = -width / 4.0f;
            float sphere1_cy = 0.0f;
            float sphere1_r = height / 5.0f;
            float sphere1_z = 3.0f;
            
            float dx1 = x - sphere1_cx;
            float dy1 = y - sphere1_cy;
            float d1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
            
            if (d1 < sphere1_r) {
                float zOffset = std::sqrt(sphere1_r * sphere1_r - d1 * d1) / sphere1_r;
                z = sphere1_z - zOffset * 0.8f;
                r = 200; g = 50; b = 50;  // Red sphere
            }
            
            // Sphere 2: Right side (closer)
            float sphere2_cx = width / 4.0f;
            float sphere2_cy = height / 8.0f;
            float sphere2_r = height / 6.0f;
            float sphere2_z = 2.0f;
            
            float dx2 = x - sphere2_cx;
            float dy2 = y - sphere2_cy;
            float d2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
            
            if (d2 < sphere2_r) {
                float zOffset = std::sqrt(sphere2_r * sphere2_r - d2 * d2) / sphere2_r;
                float newZ = sphere2_z - zOffset * 0.6f;
                if (newZ < z) {
                    z = newZ;
                    r = 50; g = 50; b = 200;  // Blue sphere
                }
            }
            
            // Box: Center
            float box_cx = 0.0f;
            float box_cy = height / 4.0f;
            float box_w = width / 8.0f;
            float box_h = height / 6.0f;
            float box_z = 4.0f;
            
            if (std::abs(x - box_cx) < box_w && std::abs(y - box_cy) < box_h) {
                if (box_z < z) {
                    z = box_z;
                    r = 200; g = 200; b = 50;  // Yellow box
                }
            }
            
            rgb.at<cv::Vec3b>(v, u) = cv::Vec3b(b, g, r);
            depth.at<float>(v, u) = z;
        }
    }
}

int main(int argc, char** argv) {
    std::cout << "Generating sample RGBD data..." << std::endl;
    
    std::string outputDir = "sample_data";
    if (argc > 1) {
        outputDir = argv[1];
    }
    
    fs::create_directories(outputDir);
    
    int width = 640;
    int height = 480;
    
    // Generate complex scene
    cv::Mat rgb, depth;
    generateComplexScene(rgb, depth, width, height);
    
    // Save RGB
    std::string rgbPath = outputDir + "/sample_rgb.png";
    if (cv::imwrite(rgbPath, rgb)) {
        std::cout << "Saved: " << rgbPath << std::endl;
    }
    
    // Save depth as 16-bit PNG (mm)
    cv::Mat depth16;
    depth.convertTo(depth16, CV_16UC1, 1000.0);  // Convert m to mm
    std::string depthPngPath = outputDir + "/sample_depth.png";
    if (cv::imwrite(depthPngPath, depth16)) {
        std::cout << "Saved: " << depthPngPath << std::endl;
    }
    
    // Save depth as NPY
    std::vector<float> depthVec(depth.begin<float>(), depth.end<float>());
    std::string depthNpyPath = outputDir + "/sample_depth.npy";
    if (rgbd::io::saveDepthNPY(depthNpyPath, depthVec, width, height)) {
        std::cout << "Saved: " << depthNpyPath << std::endl;
    }
    
    // Print example usage
    std::cout << "\nExample usage:" << std::endl;
    std::cout << "  ./rgbd_rerender \\" << std::endl;
    std::cout << "    --rgb " << rgbPath << " \\" << std::endl;
    std::cout << "    --depth " << depthNpyPath << " \\" << std::endl;
    std::cout << "    --fx 500 --fy 500 \\" << std::endl;
    std::cout << "    --focal_list 0.5,0.75,1.0,1.5,2.0 \\" << std::endl;
    std::cout << "    --out_dir output" << std::endl;
    
    std::cout << "\nOr with depth in mm:" << std::endl;
    std::cout << "  ./rgbd_rerender \\" << std::endl;
    std::cout << "    --rgb " << rgbPath << " \\" << std::endl;
    std::cout << "    --depth " << depthPngPath << " \\" << std::endl;
    std::cout << "    --depth_scale 0.001 \\" << std::endl;
    std::cout << "    --fx 500 --fy 500" << std::endl;
    
    return 0;
}
