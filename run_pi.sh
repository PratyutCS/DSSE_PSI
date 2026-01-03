#!/bin/bash

# 1. Clean up existing RocksDB directories
echo "Cleaning up database directories..."
rm -rf Sigma_map1 Server_map2 Sorted_Index_map3

# 2. Compile the project
echo "Compiling queen.cpp..."
g++ queen.cpp ./FAST/Search.cpp ./FAST/Update.cpp ./FAST/Setup.cpp ./FAST/Utilities.cpp -lcryptopp -lrocksdb -o queen

# 3. Check for compilation success
if [ $? -eq 0 ]; then
    echo "Compilation successful. Executing ./queen..."
    # 4. Execute the binary
    ./queen
else
    echo "Compilation failed."
    exit 1
fi
