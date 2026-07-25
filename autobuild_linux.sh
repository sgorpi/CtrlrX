#!/bin/bash
# If you get complaints, sudo dnf install ninja-build

# Dynamically resolve directory relative to script path
PROJECT_ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"

# Extract LuaJIT archive if it hasn't been extracted yet
LUAJIT_ZIP="$PROJECT_ROOT/Source/Resources/LuaJIT/luajit.zip"
LUAJIT_TARGET_DIR="$PROJECT_ROOT/Source/Misc/luajit"

if [ -f "$LUAJIT_ZIP" ] && [ ! -d "$LUAJIT_TARGET_DIR" ]; then
    echo "Extracting LuaJIT resource..."
    mkdir -p "$PROJECT_ROOT/Source/Misc"
    unzip -q "$LUAJIT_ZIP" -d "$PROJECT_ROOT/Source/Misc"
fi

echo "Select build type (Using Ninja):"
echo "1) Full Release (Clean + Configure + Build)"
echo "2) Full Debug (Clean + Configure + Build)"
echo "3) Clean Rebuild (Ninja Clean + Build)"
echo "4) Quick Build (Ninja Only)"
read -p "Enter choice [1-4]: " choice

case $choice in
    1|2)
        BUILD_TYPE="Release"
        [ "$choice" == "2" ] && BUILD_TYPE="Debug"

        echo "--- Starting Full $BUILD_TYPE Build ---"
        rm -rf "$BUILD_DIR"
        cmake -S "$PROJECT_ROOT" -B "$BUILD_DIR" -G Ninja -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DCTRLRX_USE_LUAJIT=ON
        cmake --build "$BUILD_DIR" -j$(nproc)
        ;;

    3)
        if [ ! -d "$BUILD_DIR" ]; then echo "Run 1 or 2 first."; exit 1; fi
        echo "--- Cleaning and Rebuilding ---"
        cmake --build "$BUILD_DIR" --target clean
        cmake --build "$BUILD_DIR" -j$(nproc)
        ;;

    4)
        if [ ! -d "$BUILD_DIR" ]; then echo "Run 1 or 2 first."; exit 1; fi
        echo "--- Fast Incremental Build ---"
        cmake --build "$BUILD_DIR" -j$(nproc)
        ;;

    *)
        echo "Invalid selection. Exiting."
        exit 1
        ;;
esac