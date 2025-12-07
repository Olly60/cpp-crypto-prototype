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
# Recursively find all .cpp files under src/ and store in SOURCES
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# Create the main executable target from the gathered source files
add_executable(${PROJECT_NAME} ${SOURCES})

# =========================================================
# Include project header files
# =========================================================
# Add include path for your own project headers
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# =========================================================
# ASIO (header-only networking library)
# =========================================================
# Add ASIO headers to include path
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/depends/asio/include)
# Define ASIO_STANDALONE for standalone mode (no Boost required)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE)

# =========================================================
# LevelDB (key-value database)
# =========================================================
# Add LevelDB as a subproject so CMake builds it as its own target
add_subdirectory(${PROJECT_SOURCE_DIR}/depends/leveldb)
# Manually add LevelDB headers (needed because LevelDB target does not propagate headers)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/depends/leveldb/include)
# Link the LevelDB library to your main executable
target_link_libraries(${PROJECT_NAME} PRIVATE leveldb)

# =========================================================
# libsodium (cryptography library)
# =========================================================
# Add libsodium as a subproject; build outputs go to a dedicated folder
add_subdirectory(${PROJECT_SOURCE_DIR}/depends/libsodium ${CMAKE_BINARY_DIR}/libsodium_build)
# Add libsodium headers to include path (in case target does not propagate headers)
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/depends/libsodium/include)
# Link the compiled libsodium library to your main executable
target_link_libraries(${PROJECT_NAME} PRIVATE sodium)
