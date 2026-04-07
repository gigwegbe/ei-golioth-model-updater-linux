#!/bin/bash
#
# Build script for the Golioth OTA Model Updater
#
# Usage:
#   ./build.sh                              # default SDK path
#   ./build.sh /path/to/golioth-firmware-sdk   # custom SDK path
#   MODEL_DIR=/opt/models ./build.sh        # custom model directory
#

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# SDK path: first argument or default
SDK_PATH="${1:-${SCRIPT_DIR}/../golioth-firmware-sdk}"

echo "========================================"
echo "Golioth OTA Model Updater - Build"
echo "========================================"
echo "SDK path:  ${SDK_PATH}"
echo "Build dir: ${BUILD_DIR}"

# Check dependencies
echo ""
echo "Checking dependencies..."

check_dep() {
    if ! command -v "$1" &> /dev/null; then
        echo "  MISSING: $1 - $2"
        return 1
    else
        echo "  OK: $1"
        return 0
    fi
}

MISSING=0
check_dep cmake "sudo apt install cmake" || MISSING=1
check_dep gcc   "sudo apt install build-essential" || MISSING=1

# Check for libcoap headers
if [ ! -f /usr/include/coap3/coap.h ] && [ ! -f /usr/local/include/coap3/coap.h ]; then
    echo "  MISSING: libcoap3-dev - sudo apt install libcoap3-dev"
    MISSING=1
else
    echo "  OK: libcoap3-dev"
fi

# Check for OpenSSL headers
if [ ! -f /usr/include/openssl/ssl.h ] && [ ! -f /usr/local/include/openssl/ssl.h ]; then
    echo "  MISSING: libssl-dev - sudo apt install libssl-dev"
    MISSING=1
else
    echo "  OK: libssl-dev"
fi

if [ "$MISSING" -eq 1 ]; then
    echo ""
    echo "Install missing dependencies:"
    echo "  sudo apt install build-essential cmake libcoap3-dev libssl-dev"
    echo ""
    exit 1
fi

# Check SDK
if [ ! -d "${SDK_PATH}" ]; then
    echo ""
    echo "Golioth SDK not found at: ${SDK_PATH}"
    echo ""
    echo "Clone it:"
    echo "  git clone https://github.com/golioth/golioth-firmware-sdk.git ${SDK_PATH}"
    echo "  cd ${SDK_PATH} && git submodule update --init --recursive"
    exit 1
fi

# Build
echo ""
echo "Building..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

CMAKE_ARGS="-DGOLIOTH_SDK_PATH=${SDK_PATH}"

if [ -n "${MODEL_DIR}" ]; then
    CMAKE_ARGS="${CMAKE_ARGS} -DMODEL_DIR=${MODEL_DIR}"
    echo "Custom model dir: ${MODEL_DIR}"
fi

cmake ${CMAKE_ARGS} ..
make -j"$(nproc)"

echo ""
echo "========================================"
echo "Build complete!"
echo "Binary: ${BUILD_DIR}/golioth_model_updater"
echo ""
echo "Run with:"
echo "  ${BUILD_DIR}/golioth_model_updater"
echo "========================================"
