#!/usr/bin/env bash
set -e

echo " SimpleFileHost — install script "
echo "==============================="

INSTALL_CMD=""
OS=$(uname -s)

if [[ "$OS" == "Linux" ]]; then
    if command -v apt &> /dev/null; then
        INSTALL_CMD="sudo apt update && sudo apt install -y zip unzip libqrencode-dev coreutils"
    elif command -v dnf &> /dev/null; then
        INSTALL_CMD="sudo dnf install -y zip unzip libqrencode-devel coreutils"
    elif command -v pacman &> /dev/null; then
        INSTALL_CMD="sudo pacman -Sy --noconfirm zip unzip libqrencode coreutils"
    fi
elif [[ "$OS" == "Darwin" ]]; then
    INSTALL_CMD="brew install zip qrencode coreutils"
else
    echo "! Unsupported OS: $OS"
    echo "You may need to manually install: zip, qrencode, coreutils"
fi

if [ -n "$INSTALL_CMD" ]; then
    echo "- Installing dependencies..."
    eval "$INSTALL_CMD"
fi

cd ..
if [ -f "simplefilehost" ]; then
    sudo cp build/simplefilehost /usr/bin/simplefilehost
    sudo chmod +x /usr/bin/simplefilehost
    echo "- Install successful!"
    echo "- Run with: simplefilehost"
else
    echo "! Install failed — binary not found."
    exit 1
fi
