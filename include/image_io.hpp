#pragma once

#include <string>
#include <vector>
#include <opencv2/core.hpp>

namespace rgbd {
namespace io {

/**
 * Load an RGB image from file
 * @param path Path to image file (PNG, JPEG, etc.)
 * @return OpenCV Mat with BGR format (OpenCV default)
 */
cv::Mat loadRGB(const std::string& path);

/**
 * Save an RGB image to file
 * @param path Output path
 * @param image RGB image data (HxWx3 uint8)
 * @param width Image width
 * @param height Image height
 * @return true on success
 */
bool saveRGB(const std::string& path, const std::vector<uint8_t>& image, 
             int width, int height);

/**
 * Save an RGB image to file
 * @param path Output path
 * @param image OpenCV Mat (BGR format)
 * @return true on success
 */
bool saveRGB(const std::string& path, const cv::Mat& image);

/**
 * Convert BGR to RGB
 */
cv::Mat bgrToRgb(const cv::Mat& bgr);

/**
 * Convert RGB to BGR
 */
cv::Mat rgbToBgr(const cv::Mat& rgb);

} // namespace io
} // namespace rgbd
