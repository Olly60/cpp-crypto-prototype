#include <filesystem>
#include <fstream>
#include <leveldb/db.h>
#include "types.h"
#include "crypto_utils.h"
#include <vector>

namespace fs = std::filesystem;

struct BlockPos {
	uint64_t file{};
	uint32_t offset{};
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

static BlockPos getBlockPos(const array256_t& blockHash) {
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
		blockPos.file = formatNumberNative<decltype(blockPos.file)>(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data()),sizeof(blockPos.file))
		);
		blockPos.offset = formatNumberNative<decltype(blockPos.offset)>(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data() + sizeof(blockPos.file)), sizeof(blockPos.offset)));
		blockPos.size = formatNumberNative<decltype(blockPos.size)>(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data() + sizeof(blockPos.file) + sizeof(blockPos.offset)), sizeof(blockPos.size)));
		return blockPos;
	}
	else { throw std::runtime_error("Corrupted value for block hash"); }


}

static void addBlock(const Block& block) {
    // Serialize the block
    const auto blockBytes = serialiseBlock(block);

    // Get the latest block's file number
    array256_t prevBlockHash = getLatestBlockHash();
    uint64_t blockFileNum = getBlockPos(prevBlockHash).file;

    // Ensure block directory exists
    fs::create_directories("chain/blocks");
    fs::path blockFilePath = fs::path("chain") / "blocks" / std::to_string(blockFileNum);

    // Determine current block file size
    std::uintmax_t blockFileSize = 0;
    if (fs::exists(blockFilePath)) {
        blockFileSize = fs::file_size(blockFilePath);
    }

    uint64_t blockOffset = blockFileSize; // Use 0-based offset

    constexpr std::size_t MAX_BLOCK_FILE_SIZE = 128'000'000;

    // Rotate block file if too big
    if (blockFileSize + blockBytes.size() > MAX_BLOCK_FILE_SIZE) {
        blockFileNum++;
        blockOffset = 0;
        blockFilePath = fs::path("chain") / "blocks" / std::to_string(blockFileNum);
    }

    // Open block file for appending binary data
    std::ofstream blockFile(blockFilePath, std::ios::binary | std::ios::app);
    if (!blockFile) {
        throw std::runtime_error("Failed to open block file: " + blockFilePath.string());
    }

    // Write block bytes
    blockFile.write(reinterpret_cast<const char*>(blockBytes.data()), blockBytes.size());

    // Compute block hash
    array256_t blockHash = getBlockHash(block);

    // Prepare LevelDB key and value
    leveldb::Slice key(reinterpret_cast<const char*>(blockHash.data()), blockHash.size());

    uint64_t blockSize = static_cast<uint64_t>(blockBytes.size());
    std::string valueString;
    valueString.append(reinterpret_cast<const char*>(&blockFileNum), sizeof(blockFileNum));
    valueString.append(reinterpret_cast<const char*>(&blockOffset), sizeof(blockOffset));
    valueString.append(reinterpret_cast<const char*>(&blockSize), sizeof(blockSize));

    leveldb::Slice value(valueString.data(), valueString.size());

    // Ensure index directory exists
    fs::create_directories("chain/index");

    // Open LevelDB with RAII
    leveldb::DB* dbRaw = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, "chain/index", &dbRaw);
    if (!status.ok() || dbRaw == nullptr) {
        throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
    }
    std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership

    // Insert block metadata
    status = db->Put(leveldb::WriteOptions(), key, value);
    if (!status.ok()) {
        throw std::runtime_error("Failed to put block metadata: " + status.ToString());
    }
}


