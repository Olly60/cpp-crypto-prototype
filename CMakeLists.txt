cmake_minimum_required(VERSION 3.20)

project(crypto VERSION 1.0 LANGUAGES CXX)

# ============================================
# Global settings
# ============================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================
# Source files
# ============================================
file(GLOB_RECURSE SOURCES "src/*.cpp")
add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME}
        PRIVATE ${PROJECT_SOURCE_DIR}/include
)

# ============================================
# ASIO (header-only)
# ============================================
target_include_directories(${PROJECT_NAME}
        PRIVATE ${PROJECT_SOURCE_DIR}/depends/asio/include
)
target_compile_definitions(${PROJECT_NAME} PRIVATE ASIO_STANDALONE)

# ============================================
# LevelDB (bundled source)
# ============================================
target_include_directories(${PROJECT_NAME}
        PRIVATE ${PROJECT_SOURCE_DIR}/depends/leveldb/include
)

file(GLOB_RECURSE LEVELDB_SOURCES "depends/leveldb/*.cc")
target_sources(${PROJECT_NAME} PRIVATE ${LEVELDB_SOURCES})

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE LEVELDB_PLATFORM_WINDOWS)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE LEVELDB_PLATFORM_POSIX)
endif()

# ============================================
# libsodium (manual, NO vcpkg)
# ============================================
# Expected structure:
# depends/libsodium/include/  (headers)
# depends/libsodium/lib/      (.lib or .a)
# depends/libsodium/bin/      (.dll or .so) OPTIONAL

# Include path
target_include_directories(${PROJECT_NAME}
        PRIVATE ${PROJECT_SOURCE_DIR}/depends/libsodium/include
)

# Library path
if(WIN32)
    # On Windows, you link the .lib file
    target_link_directories(${PROJECT_NAME}
            PRIVATE ${PROJECT_SOURCE_DIR}/depends/libsodium/lib
    )
    target_link_libraries(${PROJECT_NAME}
            PRIVATE libsodium
    )
else()
    # Linux / macOS (.a or .so)
    target_link_directories(${PROJECT_NAME}
            PRIVATE ${PROJECT_SOURCE_DIR}/depends/libsodium/lib
    )
    target_link_libraries(${PROJECT_NAME}
            PRIVATE sodium
    )
endif()
