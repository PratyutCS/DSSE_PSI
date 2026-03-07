#!/bin/bash
# setup_and_build_rocksdb.sh
# Automates: Clone, Patch, Build, and Integrate RocksDB for Android.

set -e

# 1. Configuration
ROCKSDB_VERSION="v8.10.0"
PROJECT_ROOT=$(pwd)
ROCKS_SRC_DIR="$PROJECT_ROOT/rocksdb_source"
LIBS_DEST_DIR="$PROJECT_ROOT/PI/app/src/main/cpp/libs"
INCLUDE_DEST_DIR="$PROJECT_ROOT/PI/app/src/main/cpp/include"

if [ -z "$ANDROID_NDK_HOME" ]; then
    echo "Error: ANDROID_NDK_HOME is not set."
    exit 1
fi

# 2. Clone RocksDB
if [ ! -d "$ROCKS_SRC_DIR" ]; then
    echo "Cloning RocksDB $ROCKSDB_VERSION..."
    git clone https://github.com/facebook/rocksdb.git "$ROCKS_SRC_DIR"
    cd "$ROCKS_SRC_DIR"
    git checkout "$ROCKSDB_VERSION"
else
    echo "RocksDB source already exists. Skipping clone."
    cd "$ROCKS_SRC_DIR"
fi

# 2.5 Copy Headers
echo "Copying headers to $INCLUDE_DEST_DIR..."
rm -rf "$INCLUDE_DEST_DIR/rocksdb"
mkdir -p "$INCLUDE_DEST_DIR"
cp -r "$ROCKS_SRC_DIR/include/rocksdb" "$INCLUDE_DEST_DIR/"

# 3. Build for multiple ABIs
ABIS=("arm64-v8a" "armeabi-v7a" "x86" "x86_64")
MIN_API=21

for ABI in "${ABIS[@]}"; do
    echo "================================================"
    echo "  Building RocksDB for: $ABI"
    echo "================================================"

    BUILD_DIR="$ROCKS_SRC_DIR/build_android_$ABI"
    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    cmake .. -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$MIN_API" \
        -DANDROID_STL="c++_static" \
        -DCMAKE_BUILD_TYPE=Release \
        -DPORTABLE=ON \
        -DFAIL_ON_WARNINGS=OFF \
        -DWITH_TESTS=OFF \
        -DWITH_TOOLS=OFF \
        -DWITH_BENCHMARK_TOOLS=OFF \
        -DWITH_CORE_TOOLS=OFF \
        -DWITH_GFLAGS=OFF \
        -DWITH_TBB=OFF \
        -DWITH_SNAPPY=OFF \
        -DWITH_ZLIB=OFF \
        -DWITH_LZ4=OFF \
        -DWITH_ZSTD=OFF \
        -DWITH_BZ2=OFF \
        -DUSE_RTTI=1 \
        -DCMAKE_CXX_FLAGS="-DROCKSDB_LITE=1" \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON

    ninja rocksdb

    # Copy library to destination
    mkdir -p "$LIBS_DEST_DIR/$ABI"
    cp librocksdb.a "$LIBS_DEST_DIR/$ABI/"
    echo "  -> Copied librocksdb.a to $LIBS_DEST_DIR/$ABI/"
done

# 4. Copy Headers
cd "$ROCKS_SRC_DIR"
echo "Copying headers..."
rm -rf "$INCLUDE_DEST_DIR/rocksdb"
mkdir -p "$INCLUDE_DEST_DIR"
cp -r include/rocksdb "$INCLUDE_DEST_DIR/"

echo "================================================"
echo "  ROCKSDB BUILD COMPLETE"
echo "  Libraries: $LIBS_DEST_DIR"
echo "  Headers:   $INCLUDE_DEST_DIR"
echo "================================================"
