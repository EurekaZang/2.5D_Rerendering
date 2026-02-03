#pragma once

#include <string>
#include <vector>
#include <opencv2/core.hpp>

namespace rgbd {
namespace io {

/**
 * Load a depth map from file
 * Supports: PNG (16-bit), EXR (float32), NPY
 * @param path Path to depth file
 * @param scale Scale factor to convert to meters (e.g., 0.001 for mm to m)
 * @return Depth map as float32, values in meters
 */
cv::Mat loadDepth(const std::string& path, float scale = 1.0f);

/**
 * Save depth map to EXR format (float32)
 * @param path Output path (should end with .exr)
 * @param depth Depth values in meters
 * @param width Image width
 * @param height Image height
 * @return true on success
 */
bool saveDepthEXR(const std::string& path, const std::vector<float>& depth,
                  int width, int height);

/**
 * Save depth map to EXR format (float32)
 * @param path Output path
 * @param depth OpenCV Mat (CV_32F)
 * @return true on success
 */
bool saveDepthEXR(const std::string& path, const cv::Mat& depth);

/**
 * Save depth map to PNG format (16-bit)
 * @param path Output path
 * @param depth Depth values in meters
 * @param width Image width  
 * @param height Image height
 * @param scale Scale factor (e.g., 1000 to save as mm)
 * @return true on success
 */
bool saveDepthPNG(const std::string& path, const std::vector<float>& depth,
                  int width, int height, float scale = 1000.0f);

/**
 * Save depth map to NPY format (numpy binary)
 * @param path Output path (should end with .npy)
 * @param depth Depth values
 * @param width Image width
 * @param height Image height
 * @return true on success
 */
bool saveDepthNPY(const std::string& path, const std::vector<float>& depth,
                  int width, int height);

/**
 * Load depth from NPY format
 * @param path Path to NPY file
 * @return Depth as cv::Mat (CV_32F)
 */
cv::Mat loadDepthNPY(const std::string& path);

/**
 * Save mask to PNG
 * @param path Output path
 * @param mask Binary mask (0 or 1/255)
 * @param width Image width
 * @param height Image height
 * @return true on success
 */
bool saveMask(const std::string& path, const std::vector<uint8_t>& mask,
              int width, int height);

} // namespace io
} // namespace rgbd
