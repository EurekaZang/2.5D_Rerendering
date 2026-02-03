#include "image_io.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <iostream>
#include <fstream>

namespace rgbd {
namespace io {

cv::Mat loadRGB(const std::string& path) {
    cv::Mat image = cv::imread(path, cv::IMREAD_COLOR);
    if (image.empty()) {
        std::cerr << "Error: Failed to load image: " << path << std::endl;
        return cv::Mat();
    }
    return image;
}

bool saveRGB(const std::string& path, const std::vector<uint8_t>& image,
             int width, int height) {
    if (image.size() != static_cast<size_t>(width * height * 3)) {
        std::cerr << "Error: Image size mismatch" << std::endl;
        return false;
    }
    
    // Create Mat from data (RGB format)
    cv::Mat rgb(height, width, CV_8UC3, const_cast<uint8_t*>(image.data()));
    
    // Convert RGB to BGR for OpenCV
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    
    return cv::imwrite(path, bgr);
}

bool saveRGB(const std::string& path, const cv::Mat& image) {
    if (image.empty()) {
        std::cerr << "Error: Empty image" << std::endl;
        return false;
    }
    return cv::imwrite(path, image);
}

cv::Mat bgrToRgb(const cv::Mat& bgr) {
    cv::Mat rgb;
    cv::cvtColor(bgr, rgb, cv::COLOR_BGR2RGB);
    return rgb;
}

cv::Mat rgbToBgr(const cv::Mat& rgb) {
    cv::Mat bgr;
    cv::cvtColor(rgb, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

} // namespace io
} // namespace rgbd
