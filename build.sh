#!/usr/bin/env bash
set -e

echo " SimpleFileHost — build script "
echo "==============================="

if [ ! -f "CMakeLists.txt" ]; then
    echo "! Please run this script from the root of the project (where CMakeLists.txt is)."
    exit 1
fi

# Ask about optional dependencies
read -p "Do you want to build with QR code support (requires libqrencode)? [Y/n]: " answer
case ${answer:0:1} in
    n|N )
        INSTALL_QR="NO"
        echo "- Building without QR code support"
    ;;
    * )
        INSTALL_QR="YES"
        echo "- Building with QR code support"
    ;;
esac

OS=$(uname -s)
INSTALL_CMD=""

if [[ "$OS" == "Linux" ]]; then
    if command -v apt &> /dev/null; then
        BASE_PKGS="build-essential cmake zip unzip coreutils libarchive-dev"
        QR_PKGS="libqrencode-dev"
        if [ "$INSTALL_QR" = "YES" ]; then
            INSTALL_CMD="sudo apt update && sudo apt install -y $BASE_PKGS $QR_PKGS"
        else
            INSTALL_CMD="sudo apt update && sudo apt install -y $BASE_PKGS"
        fi
    elif command -v dnf &> /dev/null; then
        BASE_PKGS="cmake gcc-c++ make zip unzip coreutils libarchive-devel"
        QR_PKGS="libqrencode-devel"
        if [ "$INSTALL_QR" = "YES" ]; then
            INSTALL_CMD="sudo dnf install -y $BASE_PKGS $QR_PKGS"
        else
            INSTALL_CMD="sudo dnf install -y $BASE_PKGS"
        fi
    elif command -v pacman &> /dev/null; then
        BASE_PKGS="cmake base-devel zip unzip coreutils libarchive"
        QR_PKGS="libqrencode"
        if [ "$INSTALL_QR" = "YES" ]; then
            INSTALL_CMD="sudo pacman -Sy --noconfirm $BASE_PKGS $QR_PKGS"
        else
            INSTALL_CMD="sudo pacman -Sy --noconfirm $BASE_PKGS"
        fi
    fi
elif [[ "$OS" == "Darwin" ]]; then
    BASE_PKGS="cmake zip coreutils libarchive"
    QR_PKGS="qrencode"
    if [ "$INSTALL_QR" = "YES" ]; then
        INSTALL_CMD="brew install $BASE_PKGS $QR_PKGS"
    else
        INSTALL_CMD="brew install $BASE_PKGS"
    fi
else
    echo "! Unsupported OS: $OS"
    echo "You may need to manually install: cmake, g++, zip, coreutils, libarchive"
    if [ "$INSTALL_QR" = "YES" ]; then
        echo "and optional: libqrencode"
    fi
fi

if [ -n "$INSTALL_CMD" ]; then
    echo "- Installing dependencies..."
    eval "$INSTALL_CMD"
fi

echo "- Building project..."
mkdir -p build
cd build

# Configure based on QR choice
if [ "$INSTALL_QR" = "YES" ]; then
    cmake ..
else
    cmake -DNO_QRENCODE=ON ..
fi

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
