#include <filesystem>
#include <fstream>
#include <leveldb/db.h>
#include "types.h"
#include "crypto_utils.h"

namespace fs = std::filesystem;

struct BlockPos {
    uint64_t file{};
    uint64_t offset{};
    uint32_t size{};
};

struct UTCXIndex {
    uint64_t amount{0};
    array256_t recipient{};
    array256_t prevTxHash{};
    uint32_t outputIndex{};
};

static array256_t getLatestBlockHash() {
    fs::create_directories("chain");
    array256_t latestBlockHash{};
    fs::path tipPath = fs::path("chain") / "tip";
    std::fstream tipFile(tipPath, std::ios::in | std::ios::binary);
    tipFile.exceptions(std::ios::failbit | std::ios::badbit);
    tipFile.read(reinterpret_cast<char*>(latestBlockHash.data()), latestBlockHash.size());
    return latestBlockHash;
}

static void setLatestBlockHash(const array256_t& blockHash) {
    fs::create_directories("chain");
    fs::path tipPath = fs::path("chain") / "tip";
    std::fstream tipFile(tipPath, std::ios::trunc);
    tipFile.exceptions(std::ios::failbit | std::ios::badbit);
    tipFile.write(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));
}

static BlockPos getBlockPos(const array256_t &blockHash) {
        fs::create_directories("chain/index");
        leveldb::Slice key(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));
        leveldb::DB* db = nullptr;
        leveldb::Options options;
        options.create_if_missing = true;

        std::unique_ptr<leveldb::DB> lifeDb(db);

        fs::path indexPath = fs::path("chain") / "index";
        leveldb::DB::Open(options, indexPath.string(), &db);

        // Get key-value
        BlockPos blockPos;
        std::string value;
        value.resize(sizeof(blockPos.file) + sizeof(blockPos.offset));
        if (db->Get(leveldb::ReadOptions(), key, &value).ok()) {
            blockPos.file = formatNumber<decltype(blockPos.file)>(reinterpret_cast<const uint8_t*>(value.data()));
            blockPos.offset = formatNumber<decltype(blockPos.offset)>(reinterpret_cast<const uint8_t*>(value.data() + sizeof(blockPos.file)));
            return blockPos;
        } else { throw std::runtime_error("Corrupted value for block hash"); }
           
           
}

static void addBlock(Block block) {

    // Tip block file
    array256_t prevBlockHash = getLatestBlockHash();
    uint64_t prevfile = getBlockPos(prevBlockHash).file;

    

    Block Bytes = serialiseBlock(block)
    // Add Block key to database
    array256_t latestBlockHash = getBlockHash(blockBytes);
    leveldb::Slice key(reinterpret_cast<const char*>(latestBlockHash.data()), sizeof(latestBlockHash));
    leveldb::Slice Value(reinterpret_cast<const char*>(blockBytes), sizeof(latestBlockHash));
    fs::create_directories("chain/index");
    leveldb::DB* db = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    std::unique_ptr<leveldb::DB> lifeDb(db); // Auto Delete DB* db
    leveldb::DB::Open(options, "chain/index", &db);
    // Put key-value
    if (!db->Put(leveldb::WriteOptions(), key, ).ok()) { throw std::runtime_error("Corrupted value for block hash"); };

    fs::create_directories("chain/blocks");

}

