#include <filesystem>
#include <fstream>
#include <leveldb/db.h>
#include "types.h"
#include "crypto_utils.h"
#include <vector>

namespace fs = std::filesystem;

static const fs::path tipPath = fs::path("blockchain") / "blockchain_tip";
static const fs::path blocksPath = fs::path("blockchain") / "blocks";
static const fs::path utxoPath = fs::path("blockchain") / "utxo";
static const fs::path undoPath = fs::path("blockchain") / "undo";
static const fs::path peersPath = fs::path("blockchain") / "peers";

// Blockchain storage management
static void addBlock(const Block& block) {
	// Serialize the block
	const auto blockBytes = serialiseBlock(block);

	// Compute block hash
	const auto blockHash = getBlockHash(block);

	// Ensure block directory exists
	fs::create_directories(blocksPath);
	const auto blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");

	// Open block file for appending binary data
	std::ofstream blockFile(blockFilePath, std::ios::binary | std::ios::app);
	if (!blockFile) {
		throw std::runtime_error("Failed to open block file: " + blockFilePath.string());
	}

	// Write block bytes
	blockFile.write(reinterpret_cast<const char*>(blockBytes.data()), blockBytes.size());

	// Update blockchain tip
	fs::create_directories(tipPath);
	std::ofstream tipFile(tipPath / "blockchain_tip", std::ios::trunc | std::ios::binary);
	tipFile.exceptions(std::ios::failbit | std::ios::badbit);
	tipFile.write(reinterpret_cast<const char*>(blockHash.data()), sizeof(blockHash));

	// Create undo file
	fs::create_directories(undoPath);
	fs::path undoFilePath = undoPath / (bytesToHex(blockHash) + ".undo");
	std::ofstream undoFile(undoFilePath, std::ios::app | std::ios::binary);
	undoFile.exceptions(std::ios::failbit | std::ios::badbit);

	// Store UTXOs for all outputs in the block
	uint32_t outputIndex = 0;



	// Write UTXO references to undo file
	for (const auto& tx : block.txs) {
		undoFile.put(tx.version); // Write transaction version
		undoFile.write(reinterpret_cast<const char*>(serialiseNumberLe(static_cast<uint32_t>(tx.txInputs.size())).data()), sizeof(tx.txInputs)); // Write input count
		for (const auto& input : tx.txInputs) {
			// Write UTXO reference
			undoFile.write(reinterpret_cast<const char*>(input.UTXOTxHash.data()), input.UTXOTxHash.size());
			auto outputIndexBytes = serialiseNumberLe(input.UTXOOutputIndex);
			undoFile.write(reinterpret_cast<const char*>(outputIndexBytes.data()), outputIndexBytes.size());
			// Get UTXO value
			UTXO utxo = getUtxoValue(input.UTXOTxHash, input.UTXOOutputIndex);
			undoFile.write(reinterpret_cast<const char*>(serialiseNumberLe(utxo.amount).data()), sizeof(utxo.amount));
			undoFile.write(reinterpret_cast<const char*>(utxo.recipient.data()), utxo.recipient.size());
		}

	}
	// Store new UTXOs
	for (const auto& tx : block.txs) {
		for (const auto& UTXO : tx.txOutputs) {
			addUtxo(UTXO, blockHash, outputIndex++); 
		}
	}
	// Remove used UTXOs
	for (const auto& tx : block.txs) {
		for (const auto& input : tx.txInputs) {
			removeUtxo(input.UTXOTxHash, input.UTXOOutputIndex); 
		}
	}
}

static void undoBlock(const Block& block) {
	// Ensure block directory exists
	fs::create_directories(blocksPath);

	// Delete block file
	fs::path blockFilePath = blocksPath / (bytesToHex(getBlockHash(block)) + ".block");
	if (fs::exists(blockFilePath)) fs::remove(blockFilePath);

	// Ensure undo directory exists
	fs::create_directories(undoPath);

	// Open undo file for reading
	fs::path undoFilePath = undoPath / (bytesToHex(getBlockHash(block)) + ".undo");
	if (!std::filesystem::exists(undoFilePath)) throw std::runtime_error("Undo file does not exist");
	std::ifstream undoFile(undoFilePath, std::ios::binary);
	undoFile.exceptions(std::ios::failbit | std::ios::badbit);

	// Read UTXO references from undo file
	for (const auto& tx : block.txs) {
		uint32_t txVersion;
		undoFile.read(reinterpret_cast<char*>(&txVersion), sizeof(txVersion)); // Read transaction version
		uint32_t utxoamount;
		undoFile.read(reinterpret_cast<char*>(&utxoamount), sizeof(utxoamount)); // Read UTXO count
		for (uint32_t i = 0; i < utxoamount; i++) {
			array256_t txHash;
			undoFile.read(reinterpret_cast<char*>(txHash.data()), txHash.size());
			uint32_t outputIndex;
			undoFile.read(reinterpret_cast<char*>(&outputIndex), sizeof(outputIndex));
			outputIndex = formatNumberNative<uint64_t>(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(outputIndex), sizeof(outputIndex)));
			uint64_t amount;
			undoFile.read(reinterpret_cast<char*>(&amount), sizeof(amount));
			amount = formatNumberNative<uint64_t>(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(&amount), sizeof(amount)));
			array256_t recipient;
			undoFile.read(reinterpret_cast<char*>(recipient.data()), recipient.size());
		}
	}
}

// UTXO storage management
static void addUtxo(const UTXO & utxo, const array256_t & txHash, const uint32_t outputIndex) {

		// Construct key
		std::string keyString;
		keyString.append(reinterpret_cast<const char*>(serialiseNumberLe(outputIndex).data()), sizeof(outputIndex));
		keyString.append(reinterpret_cast<const char*>(&txHash), sizeof(txHash));
		leveldb::Slice key(reinterpret_cast<const char*>(keyString.data()), sizeof(keyString));

		// Construct value
		std::string valueString;
		valueString.append(reinterpret_cast<const char*>(serialiseNumberLe(utxo.amount).data()), sizeof(utxo.amount));
		valueString.append(reinterpret_cast<const char*>(&utxo.recipient), sizeof(utxo.recipient));
		leveldb::Slice value(valueString.data(), valueString.size());

		// Ensure UTXO directory exists
		fs::create_directories(utxoPath);

		// Open LevelDB with RAII
		leveldb::DB* dbRaw = nullptr;
		leveldb::Options options;
		options.create_if_missing = true;

		leveldb::Status status = leveldb::DB::Open(options, (utxoPath / "leveldb").string(), &dbRaw);
		if (!status.ok() || dbRaw == nullptr) throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
		std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership

		// Insert UTXO metadata
		status = db->Put(leveldb::WriteOptions(), key, value);
		if (!status.ok()) throw std::runtime_error("Failed to put UTXO metadata: " + status.ToString());
	}

static void removeUtxo(const array256_t & txHash, const uint32_t outputIndex) {
		// Construct key
		std::string keyString;
		auto outputIndexBytes = serialiseNumberLe(outputIndex);
		keyString.append(reinterpret_cast<const char*>(outputIndexBytes.data()), sizeof(outputIndexBytes));
		keyString.append(reinterpret_cast<const char*>(&txHash), sizeof(txHash));
		leveldb::Slice key(reinterpret_cast<const char*>(keyString.data()), sizeof(keyString));

		// Ensure utxo directory exists
		fs::create_directories(utxoPath);

		// Open LevelDB with RAII
		leveldb::DB* dbRaw = nullptr;
		leveldb::Options options;
		options.create_if_missing = true;
		leveldb::Status status = leveldb::DB::Open(options, (utxoPath / "leveldb").string(), &dbRaw);
		if (!status.ok() || dbRaw == nullptr) throw std::runtime_error("Failed to open LevelDB: " + status.ToString());

		std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership

		// Delete UTXO
		status = db->Delete(leveldb::WriteOptions(), key);
		if (!status.ok()) throw std::runtime_error("Failed to delete UTXO: " + status.ToString());
	}

// Validation and retrieval
static UTXO getUtxoValue(const array256_t & txHash, const uint32_t outputIndex) {

		// Construct key
		std::string keyString;
		auto outputIndexBytes = serialiseNumberLe(outputIndex);
		keyString.append(reinterpret_cast<const char*>(outputIndexBytes.data()), sizeof(outputIndexBytes));
		keyString.append(reinterpret_cast<const char*>(&txHash), sizeof(txHash));
		leveldb::Slice key(reinterpret_cast<const char*>(keyString.data()), sizeof(keyString));

		// Ensure UTXO directory exists
		fs::create_directories(utxoPath);
		// Open LevelDB with RAII
		leveldb::DB* dbRaw = nullptr;
		leveldb::Options options;
		options.create_if_missing = true;
		leveldb::Status status = leveldb::DB::Open(options, (utxoPath / "leveldb").string(), &dbRaw);

		if (!status.ok() || dbRaw == nullptr) throw std::runtime_error("Failed to open LevelDB: " + status.ToString());

		std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership
		// Get utxo value
		std::string value;
		UTXO utxo;

		status = db->Get(leveldb::ReadOptions(), key, &value);
		if (!status.ok()) throw std::runtime_error("UTXO not found");

		utxo.amount = formatNumberNative<uint64_t>(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data()), sizeof(utxo.amount)));
		auto recipientBytes = std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(value.data() + sizeof(utxo.amount)), sizeof(utxo.recipient));
		std::memcpy(utxo.recipient.data(), recipientBytes.data(), sizeof(recipientBytes));
		return utxo;

	}

static bool utxoValid(const array256_t & txHash, const uint32_t outputIndex) {
		// Construct key
		std::string keyString;
		auto outputIndexBytes = serialiseNumberLe(outputIndex);
		keyString.append(reinterpret_cast<const char*>(outputIndexBytes.data()), sizeof(outputIndexBytes));
		keyString.append(reinterpret_cast<const char*>(&txHash), sizeof(txHash));
		leveldb::Slice key(reinterpret_cast<const char*>(keyString.data()), sizeof(keyString));

		// Ensure utxo directory exists
		fs::create_directories(utxoPath);

		// Open LevelDB with RAII
		leveldb::DB* dbRaw = nullptr;
		leveldb::Options options;
		options.create_if_missing = true;
		leveldb::Status status = leveldb::DB::Open(options, (utxoPath / "leveldb").string(), &dbRaw);
		if (!status.ok() || dbRaw == nullptr) throw std::runtime_error("Failed to open LevelDB: " + status.ToString());

		std::unique_ptr<leveldb::DB> db(dbRaw); // RAII ownership

		// Check if utxo exists
		std::string value;
		status = db->Get(leveldb::ReadOptions(), key, &value);
		return status.ok();
	}

static bool blockExists(const array256_t& blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	return fs::exists(blockFilePath);
}

static array256_t getBlockchainTip() {
	fs::create_directories(tipPath);
	array256_t latestBlockHash{};
	std::ifstream tipFile(tipPath / "blockchain_tip", std::ios::binary);
	tipFile.exceptions(std::ios::failbit | std::ios::badbit);
	tipFile.read(reinterpret_cast<char*>(latestBlockHash.data()), sizeof(latestBlockHash));
	return latestBlockHash;
}

// Peer-to-peer storage management
static void storePeerAddress(const std::string& address, const uint16_t port) {
	fs::create_directories(peersPath);
	std::ofstream peersFile(peersPath / "peers_list", std::ios::app | std::ios::binary);
	peersFile.write(address.c_str(), address.size());
	auto portBytes = serialiseNumberLe(port);
	peersFile.write(reinterpret_cast<const char*>(portBytes.data()), sizeof(portBytes));
}

