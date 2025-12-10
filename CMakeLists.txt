# =========================================================
# Minimum CMake version and project definition
# =========================================================
cmake_minimum_required(VERSION 3.20)  # Require at least CMake 3.20
project(crypto VERSION 1.0 LANGUAGES CXX)  # Project name, version, and language

# Export compile commands for IDEs (useful for CLion, VSCode, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =========================================================
# Set C++ standard
# =========================================================
set(CMAKE_CXX_STANDARD 20)            # Use C++20
set(CMAKE_CXX_STANDARD_REQUIRED ON)   # Fail if compiler doesn't support C++20

# =========================================================
# Gather all project source files
# =========================================================
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# Create the main executable target from the gathered source files
add_executable(${PROJECT_NAME} ${SOURCES})

# =========================================================
# Include project header files
# =========================================================
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# =========================================================
# ASIO (header-only networking library)
# =========================================================
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/depends/asio/include)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE)

# =========================================================
# LevelDB (source-only approach)
# =========================================================
file(GLOB LEVELDB_SOURCES
        ${PROJECT_SOURCE_DIR}/depends/leveldb/db/*.cc
        ${PROJECT_SOURCE_DIR}/depends/leveldb/table/*.cc
        ${PROJECT_SOURCE_DIR}/depends/leveldb/util/*.cc
)

add_library(leveldb STATIC ${LEVELDB_SOURCES})

target_include_directories(leveldb PUBLIC
        ${PROJECT_SOURCE_DIR}/depends/leveldb/include
)

target_link_libraries(app PRIVATE leveldb)

# =========================================================
# libsodium (cryptography library)
# =========================================================
add_subdirectory(${PROJECT_SOURCE_DIR}/depends/libsodium ${CMAKE_BINARY_DIR}/libsodium_build)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/depends/libsodium/include)
target_link_libraries(${PROJECT_NAME} PRIVATE sodium)
