#include "depth_io.hpp"
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cmath>

#ifdef HAS_OPENEXR
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfHeader.h>
#endif

namespace rgbd {
namespace io {

cv::Mat loadDepth(const std::string& path, float scale) {
    // Check file extension
    size_t dotPos = path.rfind('.');
    std::string ext = (dotPos != std::string::npos) ? path.substr(dotPos) : "";
    
    // Convert to lowercase
    for (char& c : ext) c = std::tolower(c);
    
    cv::Mat depth;
    
    if (ext == ".npy") {
        depth = loadDepthNPY(path);
    } else if (ext == ".exr") {
#ifdef HAS_OPENEXR
        // Load EXR using OpenEXR
        try {
            Imf::InputFile file(path.c_str());
            Imath::Box2i dw = file.header().dataWindow();
            int width = dw.max.x - dw.min.x + 1;
            int height = dw.max.y - dw.min.y + 1;
            
            depth.create(height, width, CV_32F);
            
            Imf::FrameBuffer frameBuffer;
            frameBuffer.insert("Y",
                Imf::Slice(Imf::FLOAT,
                    (char*)(depth.ptr<float>() - dw.min.x - dw.min.y * width),
                    sizeof(float),
                    sizeof(float) * width));
            
            file.setFrameBuffer(frameBuffer);
            file.readPixels(dw.min.y, dw.max.y);
        } catch (const std::exception& e) {
            std::cerr << "Error loading EXR: " << e.what() << std::endl;
            return cv::Mat();
        }
#else
        // Fallback: try OpenCV
        depth = cv::imread(path, cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR);
        if (depth.channels() > 1) {
            cv::extractChannel(depth, depth, 0);
        }
        depth.convertTo(depth, CV_32F);
#endif
    } else {
        // Try loading as 16-bit PNG or other format
        cv::Mat raw = cv::imread(path, cv::IMREAD_ANYDEPTH | cv::IMREAD_ANYCOLOR);
        if (raw.empty()) {
            std::cerr << "Error: Failed to load depth: " << path << std::endl;
            return cv::Mat();
        }
        
        // Convert to float
        if (raw.channels() > 1) {
            cv::extractChannel(raw, raw, 0);
        }
        raw.convertTo(depth, CV_32F);
    }
    
    // Apply scale factor
    if (scale != 1.0f && !depth.empty()) {
        depth *= scale;
    }
    
    return depth;
}

bool saveDepthEXR(const std::string& path, const std::vector<float>& depth,
                  int width, int height) {
#ifdef HAS_OPENEXR
    try {
        Imf::Header header(width, height);
        header.channels().insert("Y", Imf::Channel(Imf::FLOAT));
        
        Imf::OutputFile file(path.c_str(), header);
        Imf::FrameBuffer frameBuffer;
        
        frameBuffer.insert("Y",
            Imf::Slice(Imf::FLOAT,
                (char*)depth.data(),
                sizeof(float),
                sizeof(float) * width));
        
        file.setFrameBuffer(frameBuffer);
        file.writePixels(height);
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error saving EXR: " << e.what() << std::endl;
        return false;
    }
#else
    // Fallback: save as 32-bit TIFF
    cv::Mat depthMat(height, width, CV_32F, const_cast<float*>(depth.data()));
    std::string tiffPath = path;
    size_t dotPos = tiffPath.rfind('.');
    if (dotPos != std::string::npos) {
        tiffPath = tiffPath.substr(0, dotPos) + ".tiff";
    }
    return cv::imwrite(tiffPath, depthMat);
#endif
}

bool saveDepthEXR(const std::string& path, const cv::Mat& depth) {
    if (depth.empty() || depth.type() != CV_32F) {
        std::cerr << "Error: Invalid depth mat for EXR" << std::endl;
        return false;
    }
    
    std::vector<float> data(depth.total());
    std::memcpy(data.data(), depth.ptr<float>(), depth.total() * sizeof(float));
    
    return saveDepthEXR(path, data, depth.cols, depth.rows);
}

bool saveDepthPNG(const std::string& path, const std::vector<float>& depth,
                  int width, int height, float scale) {
    cv::Mat depth16(height, width, CV_16UC1);
    
    for (int i = 0; i < height * width; ++i) {
        float z = depth[i];
        if (std::isfinite(z) && z > 0) {
            float scaled = z * scale;
            depth16.at<uint16_t>(i / width, i % width) = 
                static_cast<uint16_t>(std::min(scaled, 65535.0f));
        } else {
            depth16.at<uint16_t>(i / width, i % width) = 0;
        }
    }
    
    return cv::imwrite(path, depth16);
}

bool saveDepthNPY(const std::string& path, const std::vector<float>& depth,
                  int width, int height) {
    // Simple NPY format writer
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file for writing: " << path << std::endl;
        return false;
    }
    
    // NPY magic number and version
    const char magic[] = "\x93NUMPY";
    file.write(magic, 6);
    
    // Version 1.0
    uint8_t version[] = {1, 0};
    file.write(reinterpret_cast<char*>(version), 2);
    
    // Header
    std::string header = "{'descr': '<f4', 'fortran_order': False, 'shape': (";
    header += std::to_string(height) + ", " + std::to_string(width) + "), }";
    
    // Pad header to make total length divisible by 64
    size_t headerLen = header.size() + 1;  // +1 for newline
    size_t paddedLen = ((10 + headerLen + 63) / 64) * 64 - 10;
    header.resize(paddedLen - 1, ' ');
    header += '\n';
    
    // Header length (little-endian 16-bit)
    uint16_t hlen = static_cast<uint16_t>(header.size());
    file.write(reinterpret_cast<char*>(&hlen), 2);
    file.write(header.c_str(), header.size());
    
    // Data
    file.write(reinterpret_cast<const char*>(depth.data()), 
               depth.size() * sizeof(float));
    
    return true;
}

cv::Mat loadDepthNPY(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open NPY file: " << path << std::endl;
        return cv::Mat();
    }
    
    // Read magic number
    char magic[6];
    file.read(magic, 6);
    if (std::strncmp(magic, "\x93NUMPY", 6) != 0) {
        std::cerr << "Error: Invalid NPY magic number" << std::endl;
        return cv::Mat();
    }
    
    // Read version
    uint8_t version[2];
    file.read(reinterpret_cast<char*>(version), 2);
    
    // Read header length
    uint16_t headerLen;
    if (version[0] == 1) {
        file.read(reinterpret_cast<char*>(&headerLen), 2);
    } else {
        uint32_t hlen32;
        file.read(reinterpret_cast<char*>(&hlen32), 4);
        headerLen = static_cast<uint16_t>(hlen32);
    }
    
    // Read header
    std::string header(headerLen, '\0');
    file.read(&header[0], headerLen);
    
    // Parse shape from header
    size_t shapePos = header.find("'shape':");
    if (shapePos == std::string::npos) {
        std::cerr << "Error: Cannot find shape in NPY header" << std::endl;
        return cv::Mat();
    }
    
    size_t startParen = header.find('(', shapePos);
    size_t endParen = header.find(')', startParen);
    std::string shapeStr = header.substr(startParen + 1, endParen - startParen - 1);
    
    // Parse dimensions
    int height = 0, width = 0;
    size_t commaPos = shapeStr.find(',');
    if (commaPos != std::string::npos) {
        height = std::stoi(shapeStr.substr(0, commaPos));
        std::string widthStr = shapeStr.substr(commaPos + 1);
        // Remove trailing comma if present
        size_t comma2 = widthStr.find(',');
        if (comma2 != std::string::npos) {
            widthStr = widthStr.substr(0, comma2);
        }
        // Trim whitespace
        widthStr.erase(0, widthStr.find_first_not_of(" \t"));
        widthStr.erase(widthStr.find_last_not_of(" \t") + 1);
        if (!widthStr.empty()) {
            width = std::stoi(widthStr);
        }
    }
    
    if (height <= 0 || width <= 0) {
        std::cerr << "Error: Invalid shape in NPY file" << std::endl;
        return cv::Mat();
    }
    
    // Read data
    cv::Mat depth(height, width, CV_32F);
    file.read(reinterpret_cast<char*>(depth.ptr<float>()), 
              height * width * sizeof(float));
    
    return depth;
}

bool saveMask(const std::string& path, const std::vector<uint8_t>& mask,
              int width, int height) {
    cv::Mat maskMat(height, width, CV_8UC1);
    
    for (int i = 0; i < height * width; ++i) {
        // Convert 0/1 to 0/255 for visibility
        maskMat.at<uint8_t>(i / width, i % width) = mask[i] ? 255 : 0;
    }
    
    return cv::imwrite(path, maskMat);
}

} // namespace io
} // namespace rgbd
