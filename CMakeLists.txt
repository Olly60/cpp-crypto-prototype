cmake_minimum_required(VERSION 3.20)

project(MyBlockchainProject VERSION 1.0 LANGUAGES CXX)

# -------------------------------
# Set C++ standard
# -------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------
# Include directories (your headers)
# -------------------------------
include_directories(${PROJECT_SOURCE_DIR}/include)

# -------------------------------
# Gather all source files
# -------------------------------
file(GLOB_RECURSE SOURCES "src/*.cpp")

# -------------------------------
# Create executable
# -------------------------------
add_executable(${PROJECT_NAME} ${SOURCES})

# -------------------------------
# Find dependencies via vcpkg
# -------------------------------
# Requires a vcpkg.json in the project root declaring dependencies
# Example vcpkg.json:
# {
#   "name": "my-blockchain-project",
#   "version": "1.0.0",
#   "dependencies": [
#     "leveldb",
#     "openssl",
#     "spdlog"
#   ]
# }

# Find LevelDB
find_package(leveldb CONFIG REQUIRED)
# Find OpenSSL
find_package(OpenSSL REQUIRED)
# Find spdlog (example header-only logger)
find_package(spdlog CONFIG REQUIRED)

# -------------------------------
# Link libraries to your executable
# -------------------------------
target_link_libraries(${PROJECT_NAME} PRIVATE
    leveldb::leveldb
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog
)

# -------------------------------
# Optional: copy DLLs for Windows if needed
# -------------------------------
# This is only needed if any of your libraries are dynamic (.dll)
# Example for OpenSSL DLLs:
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:OpenSSL::SSL>"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:OpenSSL::Crypto>"
            $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
endif()
