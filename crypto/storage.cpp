#include <filesystem>
#include <fstream>
#include <leveldb/db.h>
#include "types.h"
#include "crypto_utils.h"
#include <vector>

namespace fs = std::filesystem;

static struct BlockPosValue {
	uint64_t file{};
	uint32_t offset{};
	uint32_t size{};
};

// Define paths
static const fs::path blockchainIndexPath = fs::path("blockchain") / "blockchain_index";
static const fs::path blockchainTipPath = fs::path("blockchain") / "blockchain_tip";
static const fs::path blockchainPath = fs::path("blockchain") / "blocks";
static const fs::path blockchainUtxoPath = fs::path("blockchain") / "utxo";

static array256_t getLatestBlockHash() {
	fs::create_directories(blockchainTipPath);
	array256_t latestBlockHash{};
	std::fstream tipFile(blockchainTipPath / "blockchain_tip", std::ios::in | std::ios::binary);
	tipFile.exceptions(std::ios::failbit | std::ios::badbit);
	tipFile.read(reinterpret_cast<char*>(latestBlockHash.data()), latestBlockHash.size());
	return latestBlockHash;
}

static void setLatestBlockHash(const array256_t& blockHash) {
	fs::create_directories(blockchainTipPath);
	std::fstream tipFile(blockchainTipPath / "blockchain_tip", std::ios::trunc);
	tipFile.exceptions(std::ios::failbit | std::ios::badbit);
	tipFile.write(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));
}

static void addBlock(const Block& block) {
    // Serialize the block
    const auto blockBytes = serialiseBlock(block);

    // Get the latest block's file number
    array256_t prevBlockHash = getLatestBlockHash();
    uint64_t blockFileNum = getBlockPos(prevBlockHash).file;

    // Ensure block directory exists
    fs::create_directories(blockchainPath);
    fs::path blockFilePath = blockchainPath / std::to_string(blockFileNum);

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
        blockFilePath = blockchainPath / std::to_string(blockFileNum);
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
    fs::create_directories(blockchainIndexPath);

    // Open LevelDB with RAII
    leveldb::DB* dbRaw = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, (blockchainIndexPath / "leveldb").string(), &dbRaw);
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

static BlockPosValue getBlockPos(const array256_t& blockHash) {
	fs::create_directories(blockchainIndexPath);
	leveldb::Slice key(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));
	leveldb::DB* db = nullptr;
	leveldb::Options options;
	options.create_if_missing = true;

	std::unique_ptr<leveldb::DB> lifeDb(db);

	leveldb::DB::Open(options, blockchainIndexPath.string(), &db);

	// Get key-value
	BlockPosValue blockPos;
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

static bool blockExists(const array256_t& blockHash) {
    fs::create_directories(blockchainIndexPath);
    leveldb::Slice key(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));
    // Open LevelDB with RAII
    leveldb::DB* dbRaw = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, (blockchainIndexPath / "leveldb").string(), &dbRaw);
    if (!status.ok() || dbRaw == nullptr) {
        throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
    }
    std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership
    // Check if key exists
    std::string value;
    status = db->Get(leveldb::ReadOptions(), key, &value);
    return status.ok();
}

static void addUtxo(const UTXO& utxo, const array256_t& txHash, const uint32_t outputIndex) {

	std::string keyString; 
    keyString.append(reinterpret_cast<const char*>(serialiseNumberLe(outputIndex).data()), sizeof(outputIndex));
    keyString.append(reinterpret_cast<const char*>(&txHash), sizeof(txHash));
    leveldb::Slice key(reinterpret_cast<const char*>(keyString.data()), keyString.size());

    std::string valueString;
    valueString.append(reinterpret_cast<const char*>(serialiseNumberLe(utxo.amount).data()), sizeof(utxo.amount));
    valueString.append(reinterpret_cast<const char*>(&utxo.recipient), sizeof(utxo.recipient));
    leveldb::Slice value(valueString.data(), valueString.size());

    // Ensure utxo directory exists
    fs::create_directories(blockchainUtxoPath);

    // Open LevelDB with RAII
    leveldb::DB* dbRaw = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;

    leveldb::Status status = leveldb::DB::Open(options, (blockchainUtxoPath / "leveldb").string(), &dbRaw);
    if (!status.ok() || dbRaw == nullptr) {
        throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
    }
    std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership

    // Insert utxo metadata
    status = db->Put(leveldb::WriteOptions(), key, value);
    if (!status.ok()) {
        throw std::runtime_error("Failed to put utxo metadata: " + status.ToString());
    }
}

static void removeUtxo(const array256_t& txHash, const uint32_t outputIndex) {
	std::string keyString; 
	auto outputIndexBytes = serialiseNumberLe(outputIndex);
    keyString.append(reinterpret_cast<const char*>(outputIndexBytes.data()), sizeof(outputIndexBytes));
    keyString.append(reinterpret_cast<const char*>(&txHash), sizeof(txHash));
    leveldb::Slice key(reinterpret_cast<const char*>(keyString.data()), keyString.size());
    // Ensure utxo directory exists
    fs::create_directories(blockchainUtxoPath);
    // Open LevelDB with RAII
    leveldb::DB* dbRaw = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, (blockchainUtxoPath / "leveldb").string(), &dbRaw);
    if (!status.ok() || dbRaw == nullptr) {
        throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
    }
    std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership
    // Remove utxo metadata
    status = db->Delete(leveldb::WriteOptions(), key);
    if (!status.ok()) {
        throw std::runtime_error("Failed to delete utxo metadata: " + status.ToString());
    }
}

static UTXO getUtxoValue(const array256_t& txHash, const uint32_t outputIndex) {
    std::string keyString;
    auto outputIndexBytes = serialiseNumberLe(outputIndex);
    keyString.append(reinterpret_cast<const char*>(outputIndexBytes.data()), sizeof(outputIndexBytes));
    keyString.append(reinterpret_cast<const char*>(&txHash), sizeof(txHash));
    leveldb::Slice key(reinterpret_cast<const char*>(keyString.data()), keyString.size());
    // Ensure utxo directory exists
    fs::create_directories(blockchainUtxoPath);
    // Open LevelDB with RAII
    leveldb::DB* dbRaw = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, (blockchainUtxoPath / "leveldb").string(), &dbRaw);
    if (!status.ok() || dbRaw == nullptr) {
        throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
    }
    std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership
    // Get utxo value
    std::string value;
    UTXO utxo;
    if (db->Get(leveldb::ReadOptions(), key, &value).ok()) {
        utxo.amount = formatNumberNative<uint64_t>(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data()), sizeof(utxo.amount)));
        auto recipientBytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data() + sizeof(utxo.amount)), sizeof(utxo.recipient));
        std::memcpy(utxo.recipient.data(), recipientBytes.data(), recipientBytes.size());
        return utxo;
    }
    else {
        throw std::runtime_error("UTXO not found");
	}
}

static bool utxoExists(const array256_t& txHash, const uint32_t outputIndex) {
    std::string keyString;
    auto outputIndexBytes = serialiseNumberLe(outputIndex);
    keyString.append(reinterpret_cast<const char*>(outputIndexBytes.data()), sizeof(outputIndexBytes));
    keyString.append(reinterpret_cast<const char*>(&txHash), sizeof(txHash));
    leveldb::Slice key(reinterpret_cast<const char*>(keyString.data()), keyString.size());
    // Ensure utxo directory exists
    fs::create_directories(blockchainUtxoPath);
    // Open LevelDB with RAII
    leveldb::DB* dbRaw = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, (blockchainUtxoPath / "leveldb").string(), &dbRaw);
    if (!status.ok() || dbRaw == nullptr) {
        throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
    }
    std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership
    // Check if utxo exists
    std::string value;
    status = db->Get(leveldb::ReadOptions(), key, &value);
    return status.ok();
}

static void removeBlockUtxos(const Block& block) {
    for (const auto& tx : block.transactions) {
        // Remove UTXOs created by this transaction
        for (uint32_t outputIndex = 0; outputIndex < tx.outputs.size(); ++outputIndex) {
            const array256_t txHash = getTransactionHash(tx);
            removeUtxo(txHash, outputIndex);
        }
        // Restore UTXOs spent by this transaction
        for (const auto& txInput : tx.inputs) {
            const array256_t utxoTxHash = txInput.UTXOTxHash;
            const uint32_t utxoOutputIndex = txInput.UTXOOutputIndex;
            const UTXO utxoValue = getUtxoValue(utxoTxHash, utxoOutputIndex); // Assuming we have a way to retrieve the original UTXO value
            addUtxo(utxoValue, utxoTxHash, utxoOutputIndex);
        }
    }
}

static void removeLatestBlock( {
    array256_t latestBlockHash = getLatestBlockHash();
    BlockPosValue blockPos = getBlockPos(latestBlockHash);
    // Open block file
    fs::path blockFilePath = blockchainPath / std::to_string(blockPos.file);
    std::ifstream blockFile(blockFilePath, std::ios::binary);
    if (!blockFile) {
        throw std::runtime_error("Failed to open block file: " + blockFilePath.string());
    }
    // Seek to block position
    blockFile.seekg(blockPos.offset, std::ios::beg);
    // Read block bytes
    std::vector<uint8_t> blockBytes(blockPos.size);
    blockFile.read(reinterpret_cast<char*>(blockBytes.data()), blockPos.size);
    // Deserialize block
    Block block = formatBlock(std::span<const uint8_t>(blockBytes.data(), blockBytes.size()));
    // Remove UTXOs associated with this block
    removeBlockUtxos(block);
    // Remove block metadata from LevelDB
    fs::create_directories(blockchainIndexPath);
    leveldb::Slice key(reinterpret_cast<const char*>(latestBlockHash.data()), latestBlockHash.size());
    leveldb::DB* dbRaw = nullptr;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status status = leveldb::DB::Open(options, (blockchainIndexPath / "leveldb").string(), &dbRaw);
    if (!status.ok() || dbRaw == nullptr) {
        throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
    }
    std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership
    status = db->Delete(leveldb::WriteOptions(), key);
    if (!status.ok()) {
        throw std::runtime_error("Failed to delete block metadata: " + status.ToString());
    }
    // Update latest block hash to previous block's hash
    setLatestBlockHash(block.prevBlockHash);
	})




