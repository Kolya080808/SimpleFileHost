#!/usr/bin/env bash
set -e

echo " SimpleFileHost — cleanup "
echo "=========================="

# Удаляем артефакты сборки
sudo rm -rf /usr/bin/simplefilehost || true

echo "[OK] Binary removing complete."

OS=$(uname -s)

if [[ "$OS" == "Linux" ]]; then
    echo ""
    read -p "Remove apt packages (cmake, qrencode)? [y/N]: " ans
    if [[ "$ans" =~ ^[Yy]$ ]]; then
        sudo apt remove --purge -y cmake libqrencode-dev
        sudo apt autoremove -y
        echo "[OK] Packages removed."
    else
        echo "Skipped."
    fi
elif [[ "$OS" == "Darwin" ]]; then
    echo "macOS cleanup skipped — handled via Homebrew manually."
fi

echo ""
echo "[Done] Cleanup complete."

