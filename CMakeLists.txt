cmake_minimum_required(VERSION 3.20)

project(crypto VERSION 1.0 LANGUAGES CXX)

# Export compile commands for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===============================
# Gather project source files
# ===============================
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# Create the main executable
add_executable(${PROJECT_NAME} ${SOURCES})

# ===============================
# Project headers
# ===============================
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# ===============================
# ASIO (header-only)
# ===============================
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/depends/asio/include)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE)

# ===============================
# LevelDB (official CMake build)
# ===============================

add_subdirectory(${PROJECT_SOURCE_DIR}/depends/leveldb)
target_link_libraries(${PROJECT_NAME} PRIVATE leveldb)

# ===============================
# libsodium (official CMake build)
# ===============================

# Build libsodium as a separate library in a dedicated binary folder
add_subdirectory(${PROJECT_SOURCE_DIR}/depends/libsodium ${CMAKE_BINARY_DIR}/libsodium_build)

# Only add include path if the target does not propagate it automatically
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/depends/libsodium/include)
target_link_libraries(${PROJECT_NAME} PRIVATE sodium)
