#include "config.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>

namespace rgbd {
namespace app {

std::string Config::validate() const {
    if (rgbPath.empty()) {
        return "RGB image path is required";
    }
    if (depthPath.empty()) {
        return "Depth map path is required";
    }
    if (fx <= 0 || fy <= 0) {
        return "Focal length (fx, fy) must be positive";
    }
    if (focalScales.empty()) {
        return "At least one focal scale is required";
    }
    if (tauRel <= 0 || tauAbs <= 0) {
        return "Depth thresholds (tau_rel, tau_abs) must be positive";
    }
    if (nearPlane <= 0 || farPlane <= 0 || nearPlane >= farPlane) {
        return "Invalid near/far planes";
    }
    return "";
}

void Config::print() const {
    std::cout << "\n=== Configuration ===" << std::endl;
    std::cout << "RGB: " << rgbPath << std::endl;
    std::cout << "Depth: " << depthPath << std::endl;
    std::cout << "Output: " << outputDir << std::endl;
    std::cout << "Intrinsics: fx=" << fx << ", fy=" << fy 
              << ", cx=" << cx << ", cy=" << cy << std::endl;
    std::cout << "Depth scale: " << depthScale << std::endl;
    std::cout << "Focal scales: [";
    for (size_t i = 0; i < focalScales.size(); ++i) {
        std::cout << focalScales[i];
        if (i < focalScales.size() - 1) std::cout << ", ";
    }
    std::cout << "]" << std::endl;
    std::cout << "Thresholds: tau_rel=" << tauRel << ", tau_abs=" << tauAbs << std::endl;
    std::cout << "Planes: near=" << nearPlane << ", far=" << farPlane << std::endl;
    std::cout << "GPU device: " << gpuDevice << std::endl;
    std::cout << "=====================\n" << std::endl;
}

void printUsage(const char* programName) {
    std::cout << "RGBD Rerendering - Re-render RGBD images with different focal lengths\n\n";
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Required options:\n";
    std::cout << "  --rgb PATH          Path to RGB image\n";
    std::cout << "  --depth PATH        Path to depth map (meters or scaled)\n";
    std::cout << "  --fx VALUE          Focal length X (pixels)\n";
    std::cout << "  --fy VALUE          Focal length Y (pixels)\n\n";
    std::cout << "Optional options:\n";
    std::cout << "  --cx VALUE          Principal point X (default: image center)\n";
    std::cout << "  --cy VALUE          Principal point Y (default: image center)\n";
    std::cout << "  --out_dir PATH      Output directory (default: ./output)\n";
    std::cout << "  --depth_scale VALUE Scale to convert depth to meters (default: 1.0)\n";
    std::cout << "  --focal_list VALUES Comma-separated focal scales (default: 0.5,0.75,1.0,1.5,2.0)\n";
    std::cout << "  --tau_rel VALUE     Relative depth threshold (default: 0.05)\n";
    std::cout << "  --tau_abs VALUE     Absolute depth threshold in meters (default: 0.1)\n";
    std::cout << "  --near VALUE        Near clipping plane (default: 0.1)\n";
    std::cout << "  --far VALUE         Far clipping plane (default: 100.0)\n";
    std::cout << "  --gpu VALUE         GPU device index (default: -1 for auto)\n";
    std::cout << "  --W_out VALUE       Output width (default: same as input)\n";
    std::cout << "  --H_out VALUE       Output height (default: same as input)\n";
    std::cout << "  --save_exr          Save depth as EXR (default: true)\n";
    std::cout << "  --save_npy          Save depth as NPY (default: false)\n";
    std::cout << "  --save_png          Save depth as PNG (default: true)\n";
    std::cout << "  -h, --help          Show this help message\n";
}

static std::vector<float> parseFloatList(const std::string& str) {
    std::vector<float> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, ',')) {
        try {
            result.push_back(std::stof(item));
        } catch (...) {
            // Skip invalid values
        }
    }
    return result;
}

bool parseArgs(int argc, char** argv, Config& config) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        }
        
        // Check if we have a value following
        auto getValue = [&]() -> const char* {
            if (i + 1 < argc) {
                return argv[++i];
            }
            std::cerr << "Error: Missing value for " << arg << std::endl;
            return nullptr;
        };
        
        if (arg == "--rgb") {
            const char* val = getValue();
            if (!val) return false;
            config.rgbPath = val;
        }
        else if (arg == "--depth") {
            const char* val = getValue();
            if (!val) return false;
            config.depthPath = val;
        }
        else if (arg == "--out_dir") {
            const char* val = getValue();
            if (!val) return false;
            config.outputDir = val;
        }
        else if (arg == "--fx") {
            const char* val = getValue();
            if (!val) return false;
            config.fx = std::stof(val);
        }
        else if (arg == "--fy") {
            const char* val = getValue();
            if (!val) return false;
            config.fy = std::stof(val);
        }
        else if (arg == "--cx") {
            const char* val = getValue();
            if (!val) return false;
            config.cx = std::stof(val);
        }
        else if (arg == "--cy") {
            const char* val = getValue();
            if (!val) return false;
            config.cy = std::stof(val);
        }
        else if (arg == "--depth_scale") {
            const char* val = getValue();
            if (!val) return false;
            config.depthScale = std::stof(val);
        }
        else if (arg == "--focal_list") {
            const char* val = getValue();
            if (!val) return false;
            config.focalScales = parseFloatList(val);
        }
        else if (arg == "--tau_rel") {
            const char* val = getValue();
            if (!val) return false;
            config.tauRel = std::stof(val);
        }
        else if (arg == "--tau_abs") {
            const char* val = getValue();
            if (!val) return false;
            config.tauAbs = std::stof(val);
        }
        else if (arg == "--near") {
            const char* val = getValue();
            if (!val) return false;
            config.nearPlane = std::stof(val);
        }
        else if (arg == "--far") {
            const char* val = getValue();
            if (!val) return false;
            config.farPlane = std::stof(val);
        }
        else if (arg == "--gpu") {
            const char* val = getValue();
            if (!val) return false;
            config.gpuDevice = std::stoi(val);
        }
        else if (arg == "--W_out") {
            const char* val = getValue();
            if (!val) return false;
            config.outputWidth = std::stoi(val);
        }
        else if (arg == "--H_out") {
            const char* val = getValue();
            if (!val) return false;
            config.outputHeight = std::stoi(val);
        }
        else if (arg == "--save_exr") {
            config.saveExr = true;
        }
        else if (arg == "--save_npy") {
            config.saveNpy = true;
        }
        else if (arg == "--save_png") {
            config.savePng = true;
        }
        else {
            std::cerr << "Warning: Unknown argument: " << arg << std::endl;
        }
    }
    
    return true;
}

} // namespace app
} // namespace rgbd
