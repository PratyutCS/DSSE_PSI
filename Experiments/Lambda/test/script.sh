#!/bin/bash

# =================== CLEANUP ===================
echo "Cleaning previous binaries and database directories..."
rm -f main
rm -rf Server_map1 Server_map2 Server_map3 Sigma_map1

# =================== COMPILATION ===================
echo "Compiling source files..."
g++ -o main main.cpp \
    ../../../FAST/Setup.cpp \
    ../../../FAST/Update.cpp \
    ../../../FAST/Search.cpp \
    ../src/Setup.cpp ../src/Update.cpp ../src/Search.cpp ../src/Utilities.cpp \
    -lcryptopp -lrocksdb

if [ $? -ne 0 ]; then
    echo "Compilation failed. Exiting."
    exit 1
fi

# =================== AUTOMATED INPUT ===================
echo "Running program with automated input..."

# Inputs:
# 1   → Update
# y   → Continue
# 2   → Search
# 0   → a = 0
# 9   → b = 9
# n   → Exit
# echo -e "1\ny\n2\n0\n9999\nn" | ./main > main.txt

# echo "Execution completed. Output saved to main.txt"
