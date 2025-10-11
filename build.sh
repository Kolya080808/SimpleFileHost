#!/usr/bin/env bash
set -e

echo " SimpleFileHost — build script "
echo "==============================="

if [ ! -f "CMakeLists.txt" ]; then
    echo "! Please run this script from the root of the project (where CMakeLists.txt is)."
    exit 1
fi

OS=$(uname -s)
INSTALL_CMD=""

if [[ "$OS" == "Linux" ]]; then
    if command -v apt &> /dev/null; then
        INSTALL_CMD="sudo apt update && sudo apt install -y build-essential cmake zip unzip libqrencode-dev coreutils"
    elif command -v dnf &> /dev/null; then
        INSTALL_CMD="sudo dnf install -y cmake gcc-c++ make zip unzip libqrencode-devel coreutils"
    elif command -v pacman &> /dev/null; then
        INSTALL_CMD="sudo pacman -Sy --noconfirm cmake base-devel zip unzip libqrencode coreutils"
    fi
elif [[ "$OS" == "Darwin" ]]; then
    INSTALL_CMD="brew install cmake zip qrencode coreutils"
else
    echo "! Unsupported OS: $OS"
    echo "You may need to manually install: cmake, g++, zip, qrencode, coreutils"
fi

if [ -n "$INSTALL_CMD" ]; then
    echo "- Installing dependencies..."
    eval "$INSTALL_CMD"
fi

echo "- Building project..."
mkdir -p build
cd build
cmake ..
cmake --build . -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

cd ..
if [ -f "build/simplefilehost" ]; then
    sudo cp build/simplefilehost /usr/bin/simplefilehost
    echo "- Build successful!"
    echo "- Run with: simplefilehost"
    rm -rf build
else
    echo "! Build failed — binary not found."
    exit 1
fi

