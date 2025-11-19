#include <filesystem>
#include <fstream>
#include <leveldb/db.h>
#include "types.h"
#include "utils.h"

namespace fs = std::filesystem;

struct BlockPos {
    uint64_t file{};
    uint64_t offset{};
    uint64_t size{};
};

static array256_t getLatestBlockHash() {
    std::filesystem::create_directories("chain");
    array256_t latestBlockHash{};
    std::fstream tipFile("chain/tip", std::ios::in | std::ios::binary);
    tipFile.exceptions(std::ios::failbit | std::ios::badbit);
    tipFile.read(reinterpret_cast<char*>(latestBlockHash.data()), latestBlockHash.size());
    return latestBlockHash;
}

static void setLatestBlockHash(const array256_t& blockHash) {
    std::filesystem::create_directories("chain");
    std::fstream tipFile("chain/tip", std::ios::trunc);
    tipFile.exceptions(std::ios::failbit | std::ios::badbit);
    tipFile.write(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));
}

static BlockPos getBlock(const array256_t &blockHash) {
        std::filesystem::create_directories("chain/index");
        leveldb::Slice key(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));
        leveldb::DB* db = nullptr;
        leveldb::Options options;
        options.create_if_missing = true;

        std::unique_ptr<leveldb::DB> lifeDb(db);

        leveldb::DB::Open(options, "chain/index", &db);

        // Get key-value
        BlockPos blockPos;
        std::string value;
        value.resize(sizeof(blockPos.file) + sizeof(blockPos.offset));
        if (db->Get(leveldb::ReadOptions(), key, &value).ok()) {
            blockPos.file = formatNumber<decltype(blockPos.file)>(reinterpret_cast<const uint8_t*>(value.data()));
            blockPos.offset = formatNumber<decltype(blockPos.offset)>(reinterpret_cast<const uint8_t*>(value.data() + sizeof(blockPos.file)));
            blockPos.size = formatNumber<decltype(blockPos.size)>(reinterpret_cast<const uint8_t*>(value.data() + sizeof(blockPos.file) + sizeof(blockPos.offset)));
            return blockPos;
        } else { throw std::runtime_error("Corrupted value for block hash"); }
           
           
}

static void addBlock(const uint8_t* blockBytes) {





    array256_t blockHash = getBlockHash(blockBytes);
    leveldb::Slice key(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));
    std::filesystem::create_directories("chain/index");
    leveldb::DB* db = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;

    std::unique_ptr<leveldb::DB> lifeDb(db);

    leveldb::DB::Open(options, "chain/index", &db);

        // Put key-value
    if (db->Put(leveldb::WriteOptions(), leveldb::Slice("hello123", 12), "").ok()) {

    }
    else { throw std::runtime_error("Corrupted value for block hash"); };


		
	}

