#include <filesystem>
#include <fstream>
#include <leveldb/db.h>
#include "types.h"
#include "crypto_utils.h"
#include <vector>

namespace fs = std::filesystem;

static const fs::path blockchainTipPath = fs::path("blockchain") / "blockchain_tip";
static const fs::path blockchainPath = fs::path("blockchain") / "blocks";
static const fs::path blockchainUtxoPath = fs::path("blockchain") / "utxo";

// Blockchain tip management
static array256_t getBlockchainTip() {
	fs::create_directories(blockchainTipPath);
	array256_t latestBlockHash{};
	std::ifstream tipFile(blockchainTipPath / "blockchain_tip", std::ios::binary);
	tipFile.exceptions(std::ios::failbit | std::ios::badbit);
	tipFile.read(reinterpret_cast<char*>(latestBlockHash.data()), latestBlockHash.size());
	return latestBlockHash;
}

static void setBlockchainTip(const array256_t& blockHash) {
	fs::create_directories(blockchainTipPath);
	std::ofstream tipFile(blockchainTipPath / "blockchain_tip", std::ios::trunc | std::ios::binary);
	tipFile.exceptions(std::ios::failbit | std::ios::badbit);
	tipFile.write(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));
}

// Block management
static void addBlock(const Block& block) {
    // Serialize the block
    const auto blockBytes = serialiseBlock(block);

    // Compute block hash
    const auto blockHash = getBlockHash(block);

    // Ensure block directory exists
    fs::create_directories(blockchainPath);
	const auto blockFilePath = blockchainPath / (bytesToHex(blockHash) + ".block");

    // Open block file for appending binary data
    std::ofstream blockFile(blockFilePath, std::ios::binary | std::ios::app);
    if (!blockFile) {
        throw std::runtime_error("Failed to open block file: " + blockFilePath.string());
    }

    // Write block bytes
    blockFile.write(reinterpret_cast<const char*>(blockBytes.data()), blockBytes.size());

	// Update blockchain tip
	setBlockchainTip(blockHash);

    // Create undo file
	fs::create_directories(blockchainPath / "undo");
    fs::path undoFilePath = blockchainPath / "undo" / (bytesToHex(blockHash) + ".undo");
    std::ofstream undoFile(undoFilePath, std::ios::app | std::ios::binary);
    if (!undoFile) {
        throw std::runtime_error("Failed to create undo file: " + undoFilePath.string());
	}
    for (const auto& tx : block.txs) {
        for (const auto& input : tx.txInputs) {
            // Serialize UTXO reference
            undoFile.write(reinterpret_cast<const char*>(input.UTXOTxHash.data()), input.UTXOTxHash.size());
            auto outputIndexBytes = serialiseNumberLe(input.UTXOOutputIndex);
            undoFile.write(reinterpret_cast<const char*>(outputIndexBytes.data()), outputIndexBytes.size());
		}
    }
}

static bool blockExists(const array256_t& blockHash) {
    fs::path blockFilePath = blockchainPath / (bytesToHex(blockHash) + ".block");
    return fs::exists(blockFilePath);
}

static void undoBlock(const Block& block) {
    // Open undo file for reading
    fs::path undoFilePath = blockchainPath / "undo" / (bytesToHex(getBlockHash(block)) + ".undo");
    std::ifstream undoFile(undoFilePath, std::ios::binary);
    if (!undoFile) {
        throw std::runtime_error("Failed to open undo file: " + undoFilePath.string());
    }

    // Read UTXO references from undo file
    while (undoFile) {
        array256_t utxoTxHash;
        undoFile.read(reinterpret_cast<char*>(utxoTxHash.data()), utxoTxHash.size());
        if (undoFile.eof()) break;

        uint32_t utxoOutputIndex;
        undoFile.read(reinterpret_cast<char*>(&utxoOutputIndex), sizeof(utxoOutputIndex));

        // Remove UTXO
        removeUtxo(utxoTxHash, utxoOutputIndex);
    }
}

// UTXO management
    
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
	// Construct key
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

static void removeUtxo(const array256_t& txHash, const uint32_t outputIndex) {
    // Construct key
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

    // Delete utxo
    status = db->Delete(leveldb::WriteOptions(), key);
    if (!status.ok()) {
        throw std::runtime_error("Failed to delete utxo: " + status.ToString());
    }
}







