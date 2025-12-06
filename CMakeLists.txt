cmake_minimum_required(VERSION 3.20)

project(crypto VERSION 1.0 LANGUAGES CXX)

# Export compile commands for IntelliSense
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Gather project source files
file(GLOB_RECURSE SOURCES "src/*.cpp")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include project headers
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)

# ===============================
# LevelDB
# ===============================
# Include LevelDB headers
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/external/leveldb/include)

# Add LevelDB source files
file(GLOB_RECURSE LEVELDB_SOURCES "external/leveldb/*.cc")
target_sources(${PROJECT_NAME} PRIVATE ${LEVELDB_SOURCES})

# Optional: platform-specific LevelDB definitions
if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DLEVELDB_PLATFORM_WINDOWS)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE -DLEVELDB_PLATFORM_POSIX)
endif()

# ===============================
# ASIO (header-only)
# ===============================
# Include ASIO headers
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/external/asio/include)

# Define ASIO_STANDALONE to use standalone ASIO (no Boost)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE)
