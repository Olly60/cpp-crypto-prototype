# =========================================================
# Minimum CMake version and project definition
# =========================================================
cmake_minimum_required(VERSION 3.20)
project(crypto VERSION 1.0 LANGUAGES CXX)

# =========================================================
# Use vcpkg as a submodule
# =========================================================
set(VCPKG_DIR "${CMAKE_SOURCE_DIR}/external/vcpkg")

# Ensure vcpkg exists
if(NOT EXISTS "${VCPKG_DIR}")
    message(FATAL_ERROR "vcpkg submodule not found in ${VCPKG_DIR}. "
            "Run: git submodule update --init --recursive")
endif()

# Tell CMake to use vcpkg toolchain
set(CMAKE_TOOLCHAIN_FILE
        "${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake"
        CACHE FILEPATH ""
)

# =========================================================
# Set C++ standard
# =========================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =========================================================
# Gather project source files
# =========================================================
file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/*.cpp")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES}
        src/storage/utxo_storage.cpp)

# Include project headers
target_include_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# =========================================================
# ASIO (header-only)
# =========================================================
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE)

# =========================================================
# Find and link libraries via vcpkg
# =========================================================

# The vcpkg toolchain automatically adds its installation prefix to the search
# paths, so find_package should now locate the configuration files installed by vcpkg.

# LevelDB
# Note: The target is usually lowercase or prefixed.
find_package(leveldb CONFIG REQUIRED)

# Libsodium
# The modern target for libsodium is often called "sodium"
find_package(sodium CONFIG REQUIRED)

# Asio
# Note: Asio is often provided as a standalone target or a Boost target.
# If you installed 'asio', the target is usually 'asio'.
find_package(asio CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
        leveldb    # Use the exported target name, check vcpkg for exact name
        sodium     # Use the exported target name, check vcpkg for exact name
        asio          # Use the exported target name, check vcpkg for exact name
)
