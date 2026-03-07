#!/bin/bash
# ==============================================================================
# Crypto++ Android NDK Build Automation Script (Multi-ABI, Production-Ready)
# ==============================================================================
# 
# Purpose: Compiles Crypto++ library for Android arm64-v8a, armeabi-v7a, x86, and x86_64.
# 
# Usage: 
#   1. Set ANDROID_NDK_HOME environment variable (or specify it below).
#   2. Run: ./build-cryptopp-android.sh
# 
# Requirements:
#   - Linux/macOS
#   - Android NDK r25+ (r26 is tested)
#   - git, make, clang
# ==============================================================================

set -e # Stop on error

# --- Configuration ---
CRYPTOPP_REPO="https://github.com/weidai11/cryptopp.git"
CRYPTOPP_TAG="CRYPTOPP_8_9_0"
ANDROID_API_LEVEL=21
NDK_HOME="${ANDROID_NDK_HOME}"
BUILD_TYPE=${1:-"Release"} # Default to Release if not specified

# Define compilation flags based on build type
if [ "$BUILD_TYPE" == "Debug" ]; then
    CXXFLAGS_EXTRA="-g3 -O0 -DDEBUG"
else
    CXXFLAGS_EXTRA="-g2 -O3 -DNDEBUG"
fi

# If ANDROID_NDK_HOME is not set, try to find it in common locations
if [ -z "$NDK_HOME" ]; then
    if [[ "$OSTYPE" == "darwin"* ]]; then
        NDK_HOME="$HOME/Library/Android/sdk/ndk-bundle"
    else
        NDK_HOME="$HOME/Android/Sdk/ndk-bundle"
    fi
    # Use latest version if multiple are present
    if [ ! -d "$NDK_HOME" ]; then
        LATEST_NDK=$(ls -d $HOME/Android/Sdk/ndk/* 2>/dev/null | tail -1)
        if [ -n "$LATEST_NDK" ]; then
            NDK_HOME="$LATEST_NDK"
        fi
    fi
fi

if [ ! -d "$NDK_HOME" ]; then
    echo "Error: ANDROID_NDK_HOME not found. Please set it manually."
    exit 1
fi

echo "--- Build Environment ---"
echo "NDK_HOME: $NDK_HOME"
echo "API Level: $ANDROID_API_LEVEL"
echo "--------------------------"

# --- Setup Directory ---
BASE_DIR="$(pwd)"
BUILD_DIR="$BASE_DIR/build-cryptopp"
PROJECT_CPP_DIR="$BASE_DIR/PI/app/src/main/cpp"
OUTPUT_DIR="$PROJECT_CPP_DIR/libs"
INCLUDE_DIR="$PROJECT_CPP_DIR/include/cryptopp"

mkdir -p "$BUILD_DIR"
mkdir -p "$OUTPUT_DIR"
mkdir -p "$INCLUDE_DIR"

# --- 1. Source Acquisition ---
if [ ! -d "$BUILD_DIR/cryptopp" ]; then
    echo "Cloning Crypto++ repository..."
    git clone "$CRYPTOPP_REPO" "$BUILD_DIR/cryptopp"
    cd "$BUILD_DIR/cryptopp"
    git checkout "$CRYPTOPP_TAG"
else
    echo "Using existing Crypto++ source..."
    cd "$BUILD_DIR/cryptopp"
fi

# Copy Android CPU features from NDK (required for cpu.cpp on Android)
if [ -d "$NDK_HOME/sources/android/cpufeatures" ]; then
    echo "Copying Android CPU features from NDK..."
    cp "$NDK_HOME/sources/android/cpufeatures/cpu-features.h" .
    cp "$NDK_HOME/sources/android/cpufeatures/cpu-features.c" .
fi

# Copy headers (do it once)
cp *.h "$INCLUDE_DIR/"

# --- 2. Build for each ABI ---
ABIS=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")

for ABI in "${ABIS[@]}"; do
    echo "****************************************"
    echo "Building for ABI: $ABI"
    echo "****************************************"
    
    make distclean || true
    
    case $ABI in
        "arm64-v8a")
            TARGET_OS="android"
            TARGET_ARCH="arm64"
            TARGET_BITS=64
            TRIPLE="aarch64-linux-android"
            ;;
        "armeabi-v7a")
            TARGET_OS="android"
            TARGET_ARCH="arm"
            TARGET_BITS=32
            TRIPLE="armv7a-linux-androideabi"
            ;;
        "x86")
            TARGET_OS="android"
            TARGET_ARCH="x86"
            TARGET_BITS=32
            TRIPLE="i686-linux-android"
            ;;
        "x86_64")
            TARGET_OS="android"
            TARGET_ARCH="x86_64"
            TARGET_BITS=64
            TRIPLE="x86_64-linux-android"
            ;;
    esac

    # Set up toolchain paths
    HOST_TAG="linux-x86_64"
    if [[ "$OSTYPE" == "darwin"* ]]; then
        HOST_TAG="darwin-x86_64"
    fi
    
    TOOLCHAIN_BIN="$NDK_HOME/toolchains/llvm/prebuilt/$HOST_TAG/bin"
    
    # Path to the specific clang++ for this ABI and API
    # Note: for armv7a, the triplet is armv7a-linux-androideabi, but the clang name uses it directly
    CXX="$TOOLCHAIN_BIN/${TRIPLE}${ANDROID_API_LEVEL}-clang++"
    AR="$TOOLCHAIN_BIN/llvm-ar"
    RANLIB="$TOOLCHAIN_BIN/llvm-ranlib"

    # Export standard NDK environment variables
    export ANDROID_NDK_HOME="$NDK_HOME"
    
    # Run the make command
    # IS_ANDROID=1 is important for GNUmakefile-cross
    make -f GNUmakefile-cross \
        CXX="$CXX" \
        AR="$AR" \
        RANLIB="$RANLIB" \
        CXXFLAGS="$CXXFLAGS_EXTRA -fPIC" \
        TARGET_OS="$TARGET_OS" \
        TARGET_ARCH="$TARGET_ARCH" \
        TARGET_BITS="$TARGET_BITS" \
        ANDROID_API_LEVEL="$ANDROID_API_LEVEL" \
        IS_ANDROID=1 \
        CRYPTOPP_DISABLE_ASM=0 \
        -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

    # Verify build
    if [ ! -f "libcryptopp.a" ]; then
        echo "Error: libcryptopp.a not found for $ABI"
        exit 1
    fi
    
    # Move output to destination
    mkdir -p "$OUTPUT_DIR/$ABI"
    mv libcryptopp.a "$OUTPUT_DIR/$ABI/"
    
    echo "Build success for $ABI"
    file "$OUTPUT_DIR/$ABI/libcryptopp.a"
done

echo "----------------------------------------"
echo "Build Summary:"
echo "Include files: $INCLUDE_DIR"
for ABI in "${ABIS[@]}"; do
    if [ -f "$OUTPUT_DIR/$ABI/libcryptopp.a" ]; then
        echo " - $ABI: [OK]"
    else
        echo " - $ABI: [MISSING]"
    fi
done
echo "----------------------------------------"
