#pragma once

#include "types.hpp"
#include <string>
#include <vector>

namespace rgbd {
namespace app {

/**
 * Application configuration
 */
struct Config {
    // Input paths
    std::string rgbPath;
    std::string depthPath;
    std::string outputDir = "./output";
    
    // Source intrinsics
    float fx = 525.0f;
    float fy = 525.0f;
    float cx = -1.0f;  // -1 means use image center
    float cy = -1.0f;
    
    // Depth scale (to convert to meters)
    float depthScale = 1.0f;  // e.g., 0.001 if depth is in mm
    
    // Target focal length settings
    std::vector<float> focalScales = {0.5f, 0.75f, 1.0f, 1.5f, 2.0f};
    
    // Output resolution (0 means same as input)
    int outputWidth = 0;
    int outputHeight = 0;
    
    // Depth discontinuity thresholds
    float tauRel = 0.05f;
    float tauAbs = 0.1f;
    
    // Rendering settings
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
    int gpuDevice = -1;
    
    // Output formats
    bool saveExr = true;
    bool saveNpy = false;
    bool savePng = true;
    
    /**
     * Get depth thresholds struct
     */
    DepthThresholds getThresholds() const {
        return DepthThresholds(tauRel, tauAbs);
    }
    
    /**
     * Validate configuration
     * @return Error message (empty if valid)
     */
    std::string validate() const;
    
    /**
     * Print configuration summary
     */
    void print() const;
};

/**
 * Parse command line arguments
 * @param argc Argument count
 * @param argv Argument values
 * @param config Output configuration
 * @return true on success
 */
bool parseArgs(int argc, char** argv, Config& config);

/**
 * Print usage information
 */
void printUsage(const char* programName);

} // namespace app
} // namespace rgbd
