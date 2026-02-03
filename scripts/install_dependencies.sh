#!/bin/bash
# =============================================================================
# RGBD Rerendering - Dependency Installation Script
# For Ubuntu 22.04 LTS
# =============================================================================

set -e

echo "================================================"
echo "  Installing dependencies for RGBD Rerendering"
echo "================================================"
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
fi

# Update package lists
echo "[1/5] Updating package lists..."
$SUDO apt-get update

# Install build essentials
echo ""
echo "[2/5] Installing build tools..."
$SUDO apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git

# Install OpenGL and EGL dependencies
echo ""
echo "[3/5] Installing OpenGL/EGL dependencies..."
$SUDO apt-get install -y \
    libgl1-mesa-dev \
    libegl1-mesa-dev \
    libgles2-mesa-dev \
    mesa-utils \
    libopengl-dev

# Install OpenCV
echo ""
echo "[4/5] Installing OpenCV..."
$SUDO apt-get install -y \
    libopencv-dev

# Install optional dependencies
echo ""
echo "[5/5] Installing optional dependencies..."
$SUDO apt-get install -y \
    libopenexr-dev \
    || echo "Warning: OpenEXR not available, EXR support will be disabled"

# Verify installations
echo ""
echo "================================================"
echo "  Verifying installations"
echo "================================================"
echo ""

echo "CMake version:"
cmake --version | head -n 1

echo ""
echo "OpenCV version:"
pkg-config --modversion opencv4 2>/dev/null || pkg-config --modversion opencv 2>/dev/null || echo "OpenCV installed (version detection failed)"

echo ""
echo "EGL check:"
if [ -f /usr/lib/x86_64-linux-gnu/libEGL.so ]; then
    echo "EGL library found"
else
    echo "Warning: EGL library not found in expected location"
fi

echo ""
echo "OpenGL check:"
if command -v glxinfo &> /dev/null; then
    glxinfo | grep "OpenGL version" || echo "OpenGL version detection failed"
else
    echo "glxinfo not available (mesa-utils may not be installed)"
fi

echo ""
echo "================================================"
echo "  Installation complete!"
echo ""
echo "  Next steps:"
echo "  1. mkdir build && cd build"
echo "  2. cmake .. -DCMAKE_BUILD_TYPE=Release"
echo "  3. make -j\$(nproc)"
echo "  4. ./bin/test_rerender"
echo "================================================"
