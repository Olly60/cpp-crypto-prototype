cmake_minimum_required(VERSION 3.20)
project(crypto VERSION 1.0 LANGUAGES CXX)

# Gather source files
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include headers
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# ASIO header-only
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE)

# Find and link libraries via vcpkg
find_package(RocksDB CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
        RocksDB::rocksdb
        unofficial-sodium::sodium
        asio::asio
)
