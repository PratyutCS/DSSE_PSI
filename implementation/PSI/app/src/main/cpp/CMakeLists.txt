cmake_minimum_required(VERSION 3.22.1)
project("psi")

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/cryptopp)

add_library(${CMAKE_PROJECT_NAME} SHARED
        native-lib.cpp
        FAST/Setup.cpp
        FAST/Update.cpp
        FAST/Search.cpp
        FAST/Utilities.cpp
        Lambda_Optimized/src/Setup.cpp
        Lambda_Optimized/src/Update.cpp
        Lambda_Optimized/src/Search.cpp)
# Note: Lambda_Optimized/src/Utilities.cpp is excluded because it
# #includes ../../FAST/Utilities.cpp which is already compiled above.
# Including both would cause duplicate symbol linker errors.

target_link_libraries(${CMAKE_PROJECT_NAME}
        android
        log
        "${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/libcryptopp.a"
        "${CMAKE_SOURCE_DIR}/libs/${ANDROID_ABI}/librocksdb.a"
        )

# Ensure 16 KB page size alignment for Android 15+ compatibility
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,-z,max-page-size=16384")