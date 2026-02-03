#!/bin/bash
# =============================================================================
# RGBD Rerendering - Build Script
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Parse arguments
BUILD_TYPE="Release"
CLEAN=false
JOBS=$(nproc)

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --clean)
            CLEAN=true
            shift
            ;;
        -j)
            JOBS=$2
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--debug|--release] [--clean] [-j N]"
            exit 1
            ;;
    esac
done

echo "================================================"
echo "  Building RGBD Rerendering"
echo "  Build type: $BUILD_TYPE"
echo "  Jobs: $JOBS"
echo "================================================"
echo ""

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo "Cleaning build directory..."
    rm -rf build
fi

# Create build directory
mkdir -p build
cd build

# Configure
echo "Configuring with CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build
echo ""
echo "Building..."
make -j$JOBS

echo ""
echo "================================================"
echo "  Build complete!"
echo ""
echo "  Executables:"
echo "    ./build/bin/rgbd_rerender"
echo "    ./build/bin/test_rerender"
echo "    ./build/bin/generate_sample"
echo ""
echo "  Run tests:"
echo "    ./build/bin/test_rerender"
echo ""
echo "  Generate sample data:"
echo "    ./build/bin/generate_sample"
echo "================================================"
