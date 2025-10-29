#!/usr/bin/env bash
set -e

echo " SimpleFileHost — cleanup "
echo "=========================="


sudo rm -rf /usr/bin/simplefilehost || true
echo "[OK] Binary removing complete."

OS=$(uname -s)

if [[ "$OS" == "Linux" ]]; then
    echo ""
    read -p "Remove optional packages (libqrencode)? [y/N]: " ans
    if [[ "$ans" =~ ^[Yy]$ ]]; then
        if command -v apt &> /dev/null; then
            sudo apt remove --purge -y libqrencode-dev
        elif command -v dnf &> /dev/null; then
            sudo dnf remove -y libqrencode-devel
        elif command -v pacman &> /dev/null; then
            sudo pacman -Rs --noconfirm libqrencode
        fi
        echo "[OK] Optional packages removed."
    else
        echo "Skipped."
    fi

    echo ""
    read -p "Remove runtime dependencies (libarchive and OpenSSL dev files)? [y/N]: " ans
    if [[ "$ans" =~ ^[Yy]$ ]]; then
        if command -v apt &> /dev/null; then
            sudo apt remove --purge -y libarchive-dev libssl-dev
        elif command -v dnf &> /dev/null; then
            sudo dnf remove -y libarchive openssl-devel
        elif command -v pacman &> /dev/null; then
            sudo pacman -Rs --noconfirm libarchive openssl
        fi
        echo "[OK] Runtime dependencies removed."
    else
        echo "Skipped."
    fi

    echo ""
    read -p "Remove build dependencies (cmake, gcc, etc)? [y/N]: " ans
    if [[ "$ans" =~ ^[Yy]$ ]]; then
        if command -v apt &> /dev/null; then
            sudo apt remove --purge -y cmake build-essential
            sudo apt autoremove -y
        elif command -v dnf &> /dev/null; then
            sudo dnf remove -y cmake gcc-c++ make
        elif command -v pacman &> /dev/null; then
            sudo pacman -Rs --noconfirm cmake base-devel
        fi
        echo "[OK] Build dependencies removed."
    else
        echo "Skipped."
    fi
elif [[ "$OS" == "Darwin" ]]; then
    echo ""
    read -p "Remove optional packages (qrencode)? [y/N]: " ans
    if [[ "$ans" =~ ^[Yy]$ ]]; then
        brew remove qrencode
        echo "[OK] Optional packages removed."
    else
        echo "Skipped."
    fi

    echo ""
    read -p "Remove runtime dependencies (libarchive and OpenSSL)? [y/N]: " ans
    if [[ "$ans" =~ ^[Yy]$ ]]; then
        brew remove libarchive openssl
        echo "[OK] Runtime dependencies removed."
    else
        echo "Skipped."
    fi
fi

echo ""
echo "[Done] Cleanup complete."
