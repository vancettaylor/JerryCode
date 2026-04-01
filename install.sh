#!/bin/bash
# JerryCode — one-line install for Linux
# Usage: curl -sSL https://raw.githubusercontent.com/vancettaylor/JerryCode/main/install.sh | bash
#   or:  bash install.sh

set -e

echo "╔══════════════════════════════════════╗"
echo "║     JerryCode Installer              ║"
echo "╚══════════════════════════════════════╝"
echo ""

# ─── Check dependencies ────────────────────────────────────────
check_dep() {
    if ! command -v "$1" &>/dev/null; then
        echo "Missing: $1"
        return 1
    fi
    return 0
}

MISSING=0
check_dep cmake   || MISSING=1
check_dep g++     || MISSING=1
check_dep git     || MISSING=1

if [ $MISSING -eq 1 ]; then
    echo ""
    echo "Installing build dependencies..."
    if command -v apt-get &>/dev/null; then
        sudo apt-get update -qq
        sudo apt-get install -y -qq cmake g++ git libssl-dev >/dev/null 2>&1
    elif command -v dnf &>/dev/null; then
        sudo dnf install -y cmake gcc-c++ git openssl-devel >/dev/null 2>&1
    elif command -v pacman &>/dev/null; then
        sudo pacman -Sy --noconfirm cmake gcc git openssl >/dev/null 2>&1
    else
        echo "Error: Cannot detect package manager. Install cmake, g++, git, libssl-dev manually."
        exit 1
    fi
    echo "Dependencies installed."
fi

# ─── Clone or update ───────────────────────────────────────────
INSTALL_DIR="${JERRYCODE_DIR:-$HOME/.local/share/jerrycode}"

if [ -d "$INSTALL_DIR/.git" ]; then
    echo "Updating existing installation..."
    cd "$INSTALL_DIR"
    git pull --quiet
else
    echo "Cloning JerryCode..."
    git clone --quiet https://github.com/vancettaylor/JerryCode.git "$INSTALL_DIR"
    cd "$INSTALL_DIR"
fi

# ─── Build ─────────────────────────────────────────────────────
echo "Building (this may take a minute on first install)..."
cmake -B build/release -DCMAKE_BUILD_TYPE=Release -DCORTEX_BUILD_TESTS=OFF -DCMAKE_CXX_FLAGS="-O2" . >/dev/null 2>&1
cmake --build build/release -j"$(nproc)" 2>&1 | tail -3

# ─── Install binary ───────────────────────────────────────────
BIN_DIR="${HOME}/.local/bin"
mkdir -p "$BIN_DIR"

cp build/release/jerrycode "$BIN_DIR/jerrycode"
cp build/release/cortex_test_harness "$BIN_DIR/jerrycode-test" 2>/dev/null || true

# Make sure ~/.local/bin is in PATH
if ! echo "$PATH" | grep -q "$BIN_DIR"; then
    echo ""
    echo "Adding $BIN_DIR to PATH..."
    for rc in "$HOME/.bashrc" "$HOME/.zshrc" "$HOME/.profile"; do
        if [ -f "$rc" ]; then
            if ! grep -q "/.local/bin" "$rc"; then
                echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$rc"
            fi
        fi
    done
    export PATH="$BIN_DIR:$PATH"
fi

# ─── Generate default config ──────────────────────────────────
CONFIG_DIR="${HOME}/.config/jerrycode"
mkdir -p "$CONFIG_DIR"
if [ ! -f "$CONFIG_DIR/config.json" ]; then
    "$BIN_DIR/jerrycode" --init --project "$CONFIG_DIR" 2>/dev/null || true
    # Move the generated config
    if [ -f "$CONFIG_DIR/cortex.json" ]; then
        mv "$CONFIG_DIR/cortex.json" "$CONFIG_DIR/config.json"
    fi
fi

# ─── Verify ───────────────────────────────────────────────────
echo ""
if "$BIN_DIR/jerrycode" --help >/dev/null 2>&1; then
    echo "╔══════════════════════════════════════╗"
    echo "║     Installation successful!         ║"
    echo "╚══════════════════════════════════════╝"
    echo ""
    echo "  Binary:  $BIN_DIR/jerrycode"
    echo "  Config:  $CONFIG_DIR/config.json"
    echo "  Source:  $INSTALL_DIR"
    echo ""
    echo "  Usage:"
    echo "    jerrycode                    # Launch TUI in current directory"
    echo "    jerrycode --help             # Show options and configured models"
    echo "    jerrycode --project /path    # Work on a specific project"
    echo "    jerrycode-test \"task\" /dir   # Run a task non-interactively"
    echo ""
    echo "  Configure your LLM provider in: $CONFIG_DIR/config.json"
    echo ""
    if ! echo "$PATH" | grep -q "$BIN_DIR"; then
        echo "  NOTE: Restart your shell or run: export PATH=\"\$HOME/.local/bin:\$PATH\""
    fi
else
    echo "Error: Installation verification failed."
    exit 1
fi
