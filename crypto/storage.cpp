#include <filesystem>
#include <fstream>
#include <leveldb/db.h>
#include "crypto_utils.h"
#include "storage.h"

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

	// Create block file
	std::ofstream blockFile;
	newBlockFile(blockFile, blockHash);

	// Write block bytes
	blockFile.write(reinterpret_cast<const char*>(blockBytes.data()), blockBytes.size());

	// Update blockchain tip
	changeBlockchainTip(blockHash);

	// Create undo file
	std::ofstream undoFile;
	newUndoFile(undoFile, blockHash);


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
			TxOutput utxo = getUtxoValue(input.UTXOTxHash, input.UTXOOutputIndex);
			undoFile.write(reinterpret_cast<const char*>(serialiseNumberLe(utxo.amount).data()), sizeof(utxo.amount));
			undoFile.write(reinterpret_cast<const char*>(utxo.recipient.data()), utxo.recipient.size());
		}

	}

	// Store UTXOs for all outputs in the block
	uint32_t outputIndex = 0;
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

	// Delete block file
	deleteBlockFile(getBlockHash(block));

	// Open undo file for reading
	auto undoData = readUndoFile(getBlockHash(block));

	// Read UTXO references from undo file
	for (const auto& tx : block.txs) {
		size_t offset = 0;
		uint32_t txVersion;
		takeBytesInto(txVersion, undoData, offset); // Read transaction version
		uint32_t utxoamount;
		takeBytesInto(utxoamount, undoData, offset); // Read UTXO count
		for (uint32_t i = 0; i < utxoamount; i++) {
			Array256_t txHash;
			takeBytesInto(txHash, undoData, offset);
			uint32_t outputIndex;
			takeBytesInto(outputIndex, undoData, offset);
			uint64_t amount;
			takeBytesInto(amount, undoData, offset);
			Array256_t recipient;
			takeBytesInto(recipient, undoData, offset);
		}
	}
}

// UTXO storage management
static void addUtxo(const TxOutput& utxo, const Array256_t& txHash, const uint32_t outputIndex) {

	// Construct key
	std::string keyString;
	appendBytes(keyString, outputIndex);
	appendBytes(keyString, txHash);
	leveldb::Slice key(keyString);

	// Construct value
	std::string valueString;
	appendBytes(valueString, utxo.amount);
	appendBytes(valueString, utxo.recipient);
	leveldb::Slice value(valueString);

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

static void removeUtxo(const Array256_t& txHash, const uint32_t outputIndex) {
	// Construct key
	std::string keyString;
	appendBytes(keyString, outputIndex);
	appendBytes(keyString, txHash);
	leveldb::Slice key(keyString);

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
static TxOutput getUtxoValue(const Array256_t& txHash, const uint32_t outputIndex) {

	// Construct key
	std::string keyString;
	appendBytes(keyString, outputIndex);
	appendBytes(keyString, txHash);
	leveldb::Slice key(keyString);

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
	TxOutput utxo;

	status = db->Get(leveldb::ReadOptions(), key, &value);
	if (!status.ok()) throw std::runtime_error("UTXO not found");

	size_t offset = 0;
	takeBytesInto(utxo.amount, { reinterpret_cast<const uint8_t*>(value.data()), value.size() }, offset);
	takeBytesInto(utxo.recipient, { reinterpret_cast<const uint8_t*>(value.data()), value.size() }, offset);
	return utxo;

}

static bool utxoValid(const Array256_t& txHash, const uint32_t outputIndex) {
	// Construct key
	std::string keyString;
	appendBytes(keyString, outputIndex);
	appendBytes(keyString, txHash);
	leveldb::Slice key(keyString);

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

// validation and retrieval
static bool blockExists(const Array256_t& blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	return fs::exists(blockFilePath);
}

Array256_t getBlockchainTip() {
	fs::create_directories(tipPath);
	Array256_t latestBlockHash{};
	std::ifstream tipFile(tipPath / "blockchain_tip", std::ios::binary);
	tipFile.exceptions(std::ios::failbit | std::ios::badbit);
	tipFile.read(reinterpret_cast<char*>(latestBlockHash.data()), sizeof(latestBlockHash));
	return latestBlockHash;
}

// Undo file management
static void newUndoFile(std::ofstream& undoFile, Array256_t blockHash) {
	fs::create_directories(undoPath);  // ensure directory exists

	const fs::path undoFilePath = undoPath / (bytesToHex(blockHash) + ".undo");

	undoFile.open(undoFilePath, std::ios::app | std::ios::binary);
	undoFile.exceptions(std::ios::failbit | std::ios::badbit);  // throw on failure
}

static void newBlockFile(std::ofstream& blockFile, const Array256_t blockHash) {
	// Ensure block directory exists
	fs::create_directories(blocksPath);
	const fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");

	// Open block file for appending binary data
	blockFile.open(blockFilePath, std::ios::binary | std::ios::app);
	blockFile.exceptions(std::ios::failbit | std::ios::badbit);
}

static void changeBlockchainTip(const Array256_t newTip) {
	fs::create_directories(tipPath);
	std::ofstream tipFile(tipPath / "blockchain_tip", std::ios::trunc | std::ios::binary);
	tipFile.exceptions(std::ios::failbit | std::ios::badbit);
	try {
		tipFile.write(reinterpret_cast<const char*>(newTip.data()), sizeof(newTip));
	}
	catch (const std::ios_base::failure& e) {
		throw std::runtime_error("Failed to update blockchain tip: " + std::string(e.what()));
	}
}

static std::vector<uint8_t> readUndoFile(const Array256_t blockHash) {
	fs::path undoFilePath = undoPath / (bytesToHex(blockHash) + ".undo");
	if (!fs::exists(undoFilePath)) throw std::runtime_error("Undo file does not exist");

	std::ifstream undoFile(undoFilePath, std::ios::binary | std::ios::ate);
	undoFile.exceptions(std::ios::failbit | std::ios::badbit);

	try {
		auto fileSize = undoFile.tellg();
		if (fileSize < 0) throw std::runtime_error("Failed to determine file size");

		std::vector<uint8_t> undoData(static_cast<size_t>(fileSize));
		undoFile.seekg(0, std::ios::beg);
		undoFile.read(reinterpret_cast<char*>(undoData.data()), fileSize);

		return undoData;
	}
	catch (const std::ios_base::failure& e) {
		throw std::runtime_error("Failed to read undo file: " + std::string(e.what()));
	}
}

static std::vector<uint8_t> readBlockFile(const Array256_t blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	if (!fs::exists(blockFilePath)) throw std::runtime_error("Block file does not exist");

	std::ifstream blockFile(blockFilePath, std::ios::binary | std::ios::ate);
	blockFile.exceptions(std::ios::failbit | std::ios::badbit);

	auto fileSize = blockFile.tellg();
	if (fileSize < 0) throw std::runtime_error("Failed to determine file size");

	std::vector<uint8_t> blockData(static_cast<size_t>(fileSize));
	blockFile.seekg(0, std::ios::beg);
	blockFile.read(reinterpret_cast<char*>(blockData.data()), fileSize);

	return blockData;
}

static void deleteUndoFile(const Array256_t blockHash) {
	fs::path undoFilePath = undoPath / (bytesToHex(blockHash) + ".undo");
	if (fs::exists(undoFilePath)) fs::remove(undoFilePath);
}

static void deleteBlockFile(const Array256_t blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	if (fs::exists(blockFilePath)) fs::remove(blockFilePath);
}

// Peer-to-peer storage management
void storePeerAddress(const std::string& address, uint16_t port) {
	fs::create_directories(peersPath);

	std::ofstream peersFile(peersPath / "peers_list",
		std::ios::app | std::ios::binary);
	peersFile.exceptions(std::ios::failbit | std::ios::badbit);
	try {

		// Write address length
		uint16_t addrLen = static_cast<uint16_t>(address.size());
		std::vector<uint8_t> tmp;

		appendBytes(tmp, addrLen);
		peersFile.write(reinterpret_cast<const char*>(tmp.data()), tmp.size());

		// Write address string bytes
		peersFile.write(address.data(), address.size());

		// Write port (LE)
		tmp.clear();
		appendBytes(tmp, port);
		peersFile.write(reinterpret_cast<const char*>(tmp.data()), tmp.size());
	}
	catch (const std::ios_base::failure& e) {
		throw std::runtime_error("Failed to store peer address: " + std::string(e.what()));
	}
}

std::vector<std::pair<std::string, uint16_t>> loadPeers() {
	std::vector<std::pair<std::string, uint16_t>> peers;

	std::ifstream peersFile(peersPath / "peers_list", std::ios::binary);
	if (!peersFile) return peers;

	// Enable exceptions for failbit and badbit
	peersFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	try {
		while (true) {
			// Read address length
			uint16_t len{};
			std::vector<uint8_t> buffer(sizeof(len));
			peersFile.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
			takeBytesInto(len, buffer);  // converts from LE to host order

			// Read address string
			std::string addr(len, '\0');
			peersFile.read(addr.data(), len);

			// Read port
			uint16_t port{};
			buffer.resize(sizeof(port));
			peersFile.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
			takeBytesInto(port, buffer);  // converts from LE to host order

			peers.emplace_back(std::move(addr), port);
		}
	}
	catch (const std::ios_base::failure&) {
		throw std::runtime_error("Failed to load peers list");
	}

	return peers;
}



