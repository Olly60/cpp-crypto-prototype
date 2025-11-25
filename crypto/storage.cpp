#include <filesystem>
#include <fstream>
#include <leveldb/db.h>
#include "crypto_utils.h"
#include "storage.h"
#include <algorithm>
#include <unordered_map>

#include "network.h"

namespace fs = std::filesystem;
static const fs::path blockchainPath = fs::path("blockchain");

static const fs::path tipPath = blockchainPath / "blockchain_tip";
static const fs::path tipFilePath = tipPath / "blockchain_tip";
static const fs::path blocksPath = blockchainPath / "blocks";
static const fs::path utxoPath = blockchainPath / "utxo";
static const fs::path undoPath = blockchainPath / "undo";
static const fs::path peersPath = blockchainPath / "peers";
static const fs::path peersFilePath = peersPath / "peers_list";

template <typename T>
requires std::is_integral_v<T>
static void numIntoFile(const T& number, std::ofstream& file) {

	// Inline little-endian serialization
	std::array<char, sizeof(T)> bytes{};
	std::memcpy(bytes.data(), &number, sizeof(T));
	// If host is big-endian, swap the bytes read from little-endian storage
	if constexpr (!isLittleEndian()) {
		std::reverse(bytes.begin(), bytes.end());
	}
	file.write(bytes.data(), bytes.size());
}



// ==========================================================
// Block storage management
// ==========================================================

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

		numIntoFile(tx.version, undoFile); // Write transaction version

		numIntoFile((tx.txInputs.size()), undoFile); // Write input count

		// For each input, write UTXO reference and value
		for (const auto& input : tx.txInputs) {
			// Write UTXO reference
			undoFile.write(reinterpret_cast<const char*>(input.UTXOTxHash.data()), input.UTXOTxHash.size());
			numIntoFile((input.UTXOOutputIndex), undoFile);

			// Retrieve UTXO value
			auto usedUtxo = getUtxoValue(input.UTXOTxHash, input.UTXOOutputIndex);
			numIntoFile((usedUtxo.amount), undoFile);
			undoFile.write(reinterpret_cast<const char*>(usedUtxo.recipient.data()), usedUtxo.recipient.size());
		}

	}

	// Store UTXOs for all outputs in the block
	uint32_t outputIndex = 0;
	// Store new UTXOs
	for (const auto& tx : block.txs) {
		for (const auto& UTXO : tx.txOutputs) {
			addUtxo(blockHash, outputIndex++, UTXO);
		}
	}
	// Remove used UTXOs
	for (const auto& tx : block.txs) {
		for (const auto& input : tx.txInputs) {
			removeUtxo(input.UTXOTxHash, input.UTXOOutputIndex);
		}
	}
}

static void undoBlock() {

	auto tipHash = getBlockchainTip();
	// Delete block file
	deleteBlockFile(tipHash);

	// Open undo file for reading
	auto undoDataBytes = readUndoFile(tipHash);
	auto blockBytes = readBlockFile(tipHash);
	auto block = formatBlock(blockBytes);

	// Read UTXO references from undo file
	for (const auto& tx : block.txs) {
		size_t offset = 0;
		uint32_t txVersion;
		takeBytesInto(txVersion, undoDataBytes, offset); // Read transaction version
		uint32_t utxoamount;
		takeBytesInto(utxoamount, undoDataBytes, offset); // Read UTXO count
		for (uint32_t i = 0; i < utxoamount; i++) {
			Array256_t txHash;
			takeBytesInto(txHash, undoDataBytes, offset);
			uint32_t outputIndex;
			takeBytesInto(outputIndex, undoDataBytes, offset);
			uint64_t amount;
			takeBytesInto(amount, undoDataBytes, offset);
			Array256_t recipient;
			takeBytesInto(recipient, undoDataBytes, offset);
			addUtxo(txHash, outputIndex, { amount, recipient });
		}
	}
}

static bool blockExists(const Array256_t& blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	return fs::exists(blockFilePath);
}

static void newBlockFile(std::ofstream& blockFile, const Array256_t blockHash) {
	// Ensure block directory exists
	fs::create_directories(blocksPath);
	const fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");

	// Open block file for appending binary data
	blockFile.open(blockFilePath, std::ios::binary | std::ios::app);
	blockFile.exceptions(std::ios::failbit | std::ios::badbit);
}

static std::vector<uint8_t> readBlockFile(const Array256_t blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	if (!fs::exists(blockFilePath)) throw std::runtime_error("Block file does not exist");
	std::ifstream blockFile(blockFilePath, std::ios::binary);
	blockFile.exceptions(std::ios::failbit | std::ios::badbit);
	try {
		// Read file contents into vector
		std::vector<uint8_t> blockData((std::istreambuf_iterator<char>(blockFile)), std::istreambuf_iterator<char>());
		return blockData;
	}
	catch (const std::ios::failure& e) {
		throw std::runtime_error("Failed to read block file: " + std::string(e.what()));
	}
}

static void deleteBlockFile(const Array256_t blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	if (fs::exists(blockFilePath)) fs::remove(blockFilePath);
}

// ===========================================================
// UTXO storage management
// ===========================================================

static void addUtxo(const Array256_t& txHash, const uint32_t outputIndex, const TxOutput& utxo) {

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

// ===========================================================
// Blockchain tip management
// ===========================================================

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

Array256_t getBlockchainTip() {
	fs::create_directories(tipPath);
	if (!fs::exists(tipFilePath)) throw std::runtime_error("Blockchain tip file does not exist");
	std::ifstream tipFile(tipFilePath, std::ios::binary);
	tipFile.exceptions(std::ios::failbit | std::ios::badbit);
	try {
		Array256_t latestBlockHash{};
		tipFile.read(reinterpret_cast<char*>(latestBlockHash.data()), sizeof(latestBlockHash));
		return latestBlockHash;
	}
	catch (const std::ios_base::failure& e) {
		throw std::runtime_error("Failed to read blockchain tip: " + std::string(e.what()));
	}
}

// ===========================================================
// Undo file management
// ===========================================================

static void newUndoFile(std::ofstream& undoFile, Array256_t blockHash) {
	fs::create_directories(undoPath);  // ensure directory exists

	const fs::path undoFilePath = undoPath / (bytesToHex(blockHash) + ".undo");

	undoFile.open(undoFilePath, std::ios::app | std::ios::binary);
	undoFile.exceptions(std::ios::failbit | std::ios::badbit);  // throw on failure
}

static std::vector<uint8_t> readUndoFile(const Array256_t blockHash) {
	fs::path undoFilePath = undoPath / (bytesToHex(blockHash) + ".undo");
	if (!fs::exists(undoFilePath)) throw std::runtime_error("Undo file does not exist");
	std::ifstream undoFile(undoFilePath, std::ios::binary);
	undoFile.exceptions(std::ios::failbit | std::ios::badbit);
	try {
		// Read file contents into vector
		std::vector<uint8_t> undoData((std::istreambuf_iterator<char>(undoFile)), std::istreambuf_iterator<char>());
		return undoData;
	}
	catch (const std::ios::failure& e) {
		throw std::runtime_error("Failed to read undo file: " + std::string(e.what()));
	}
}

static void deleteUndoFile(const Array256_t blockHash) {
	fs::path undoFilePath = undoPath / (bytesToHex(blockHash) + ".undo");
	if (fs::exists(undoFilePath)) fs::remove(undoFilePath);
}

// ===========================================================
// Peer address storage management
// ===========================================================

void storePeerAddress(const PeerAddress& peerAddr, const PeerStatus& peerStatus) {
	fs::create_directories(peersPath);
	std::ofstream peersFile(peersFilePath, std::ios::app | std::ios::binary);

	peersFile.exceptions(std::ios::failbit | std::ios::badbit);
	try {
		// Write address string bytes
		peersFile.write(peerAddr.address.data(), peerAddr.address.size());

		// Write port
		numIntoFile(peerAddr.port, peersFile);

		// Write services
		numIntoFile(peerStatus.services, peersFile);

		// Write last seen
		numIntoFile(peerStatus.lastSeen, peersFile);
	}
	catch (const std::ios_base::failure& e) {
		throw std::runtime_error("Failed to store peer address: " + std::string(e.what()));
	}
}

std::unordered_map<PeerAddress, PeerStatus> loadPeers() {
	std::unordered_map<PeerAddress, PeerStatus> peers;

	if (!fs::exists(peersFilePath)) return peers;
	std::ifstream peersFile(peersFilePath, std::ios::binary);

	// Enable exceptions for failbit and badbit
	peersFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	std::vector<uint8_t> undoData((std::istreambuf_iterator<char>(peersFile)), std::istreambuf_iterator<char>());

	try {
		while (true) {
			PeerAddress peerAddr;
			size_t offset = 0;

			// Read address string
			std::string addr(addrLen, '\0');
			takeBytesInto(addr, undoData, offset);

			// Read port
			takeBytesInto(peer.port, undoData, offset);
			PeerStatus peerStatus;
			
			
			// Read services
			takeBytesInto(peerStatus.services, undoData, offset);

			// Read last seen
			takeBytesInto(peerStatus.lastSeen, undoData, offset);

			peers.insert({ peerAddr, peerStatus });
		}
	}
	catch (const std::ios_base::failure&) {
		throw std::runtime_error("Failed to load peers list");
	}

	return peers;
}



