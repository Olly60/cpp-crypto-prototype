cmake_minimum_required(VERSION 3.20)

project(crypto VERSION 1.0 LANGUAGES CXX)

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===============================
# Gather project source files
# ===============================
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# Create executable
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
add_subdirectory(${PROJECT_SOURCE_DIR}/depends/libsodium ${CMAKE_BINARY_DIR}/libsodium_build)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/depends/libsodium/include)
target_link_libraries(${PROJECT_NAME} PRIVATE sodium)
