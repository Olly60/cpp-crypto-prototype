cmake_minimum_required(VERSION 3.20)
project(crypto VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Gather source files
add_executable(crypto
        src/main.cpp
        src/crypto_utils.cpp
        src/network/handle.cpp
        src/network/network_main.cpp
        src/network/network_utils.cpp
        src/network/request.cpp
        src/storage/block/block_heights.cpp
        src/storage/block/block_indexes.cpp
        src/storage/block/genesis_block.cpp
        src/storage/peers.cpp
        src/storage/storage_utils.cpp
        src/storage/utxo_storage.cpp
        src/verify.cpp
        src/tip.cpp
        src/user_commands.cpp
        src/user_commands.cpp
        src/block_work.cpp
        src/transaction.cpp
        src/block.cpp
)

# Include headers
target_include_directories(${PROJECT_NAME} PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

# Find and link libraries via vcpkg
find_package(RocksDB CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(Boost CONFIG REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE
        RocksDB::rocksdb
        unofficial-sodium::sodium
        asio::asio
        Boost::boost)
