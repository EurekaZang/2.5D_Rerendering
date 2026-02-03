#!/bin/bash
# =============================================================================
# RGBD Rerendering - Quick Demo Script
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

echo "================================================"
echo "  RGBD Rerendering Demo"
echo "================================================"
echo ""

# Check if built
if [ ! -f "build/bin/rgbd_rerender" ]; then
    echo "Building project first..."
    ./scripts/build.sh
fi

# Generate sample data
echo ""
echo "[1/2] Generating sample data..."
./build/bin/generate_sample sample_data

# Run rerendering
echo ""
echo "[2/2] Running rerendering with multiple focal lengths..."
./build/bin/rgbd_rerender \
    --rgb sample_data/sample_rgb.png \
    --depth sample_data/sample_depth.npy \
    --fx 500 --fy 500 \
    --focal_list 0.5,0.75,1.0,1.25,1.5,2.0 \
    --tau_rel 0.05 --tau_abs 0.1 \
    --out_dir output

echo ""
echo "================================================"
echo "  Demo complete!"
echo ""
echo "  Output files in: output/"
echo "  - scale_X.XX_rgb.png   : Re-rendered RGB"
echo "  - scale_X.XX_depth.png : Re-rendered depth (16-bit)"
echo "  - scale_X.XX_mask.png  : Validity mask"
echo "================================================"
