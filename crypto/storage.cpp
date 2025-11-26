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

static const fs::path blockHashesFilePath = blockchainPath / "blockchain_tip" / "blockchain_tip";
static const fs::path blocksPath = blockchainPath / "blocks";
static const fs::path utxoPath = blockchainPath / "utxo";
static const fs::path undoPath = blockchainPath / "undo";
static const fs::path peersFilePath = blockchainPath / "peers" / "peers_list";

// ==========================================================
// File I/O utilities
// ==========================================================

// Open a file for appending, creating directories as needed
static std::ofstream openFileForAppend(const fs::path& path) {
	try {
		fs::create_directories(path.parent_path());
		std::ofstream file(path, std::ios::app | std::ios::binary);
		file.exceptions(std::ios::failbit | std::ios::badbit);
		return file;
	}
	catch (const std::ios_base::failure& e) {
		throw std::runtime_error("Failed to open file '" + path.string() + "': " + e.what());
	}
	catch (const fs::filesystem_error& e) {
		throw std::runtime_error("Filesystem error for path '" + path.string() + "': " + e.what());
	}
}

// Append bytes of data to output container
// Append an object to an already-open file safely
template <typename T>
void appendToFile(std::ofstream& file, const T& obj) {
	try {
		std::vector<uint8_t> buffer;
		appendBytes(buffer, obj);
		file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
	}
	catch (const std::ios_base::failure& e) {
		throw std::runtime_error("Failed to append to file: " + std::string(e.what()));
	}
}

static void deleteFile(const fs::path& filePath) {
	if (fs::exists(filePath)) fs::remove(filePath);
}

static std::vector<uint8_t> readWholeFile(const fs::path& filePath) {
	if (!fs::exists(filePath))
		throw std::runtime_error("File does not exist: " + filePath.string());

	std::ifstream file;
	file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	try {
		file.open(filePath, std::ios::binary | std::ios::ate);

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		if (size < 0)
			throw std::runtime_error("Failed to determine file size: " + filePath.string());

		std::vector<uint8_t> buffer(static_cast<size_t>(size));
		file.read(reinterpret_cast<char*>(buffer.data()), size);

		return buffer;
	}
	catch (const std::ios_base::failure& e) {
		throw std::runtime_error("Failed to read file " + filePath.string() + ": " + e.what());
	}
}

std::vector<uint8_t> readBlockFile(const Array256_t& blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	return readWholeFile(blockFilePath);
}

std::vector<uint8_t> readBlockFileHeader(const Array256_t& blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	auto blockBytes = readWholeFile(blockFilePath);
	// Extract header bytes
	size_t offset = 0;
	std::vector<uint8_t> headerBytes;
	headerBytes.reserve(sizeof(decltype(BlockHeader::version)) + sizeof(decltype(BlockHeader::prevBlockHash)) + sizeof(decltype(BlockHeader::merkleRoot)) + sizeof(decltype(BlockHeader::timestamp)) + sizeof(decltype(BlockHeader::difficulty)) + sizeof(decltype(BlockHeader::nonce))); // Size of BlockHeader fields
	// Copy header fields
	headerBytes.insert(headerBytes.end(), blockBytes.begin(), blockBytes.begin() + headerBytes.capacity());
	return headerBytes;
}

// ==========================================================
// Block storage management
// ==========================================================

void addBlock(const Block& block) {

	// Serialize the block
	const auto blockBytes = serialiseBlock(block);

	// Compute block hash
	const auto blockHash = getBlockHash(block);

	auto blockFile = openFileForAppend(blocksPath / (bytesToHex(blockHash) + ".block"));

	// Write block bytes
	appendBytes(blockFile, blockBytes);

	// Update blockchain tip
	addBlockchainTip(blockHash);

	// Create undo file
	auto undoFile = openFileForAppend(undoPath / (bytesToHex(blockHash) + ".undo"));

	auto utxoDb = openUtxoDb(); // Ensure UTXO DB is initialized
	// Write UTXO references to undo file
	for (const auto& tx : block.txs) {

		appendToFile(undoFile, tx.version); // Write transaction version

		appendToFile(undoFile, (tx.txInputs.size())); // Write input count

		// For each input, write UTXO reference and value
		for (const auto& input : tx.txInputs) {
			// Write UTXO reference
			appendToFile(undoFile, input.UTXOTxHash);
			appendToFile(undoFile, input.UTXOOutputIndex);

			// Retrieve UTXO value
			auto usedUtxo = getUtxoValue(*utxoDb, input);
			appendToFile(undoFile, usedUtxo.amount);
			appendToFile(undoFile, usedUtxo.recipient);
		}

	}

	// Store UTXOs for all outputs in the block
	uint32_t outputIndex = 0;
	// Store new UTXOs
	for (const auto& tx : block.txs) {
		auto txHash = getTxHash(tx);
		for (const auto& UTXO : tx.txOutputs) {
			putUtxo(*utxoDb, { txHash, outputIndex++ }, UTXO);
		}
	}
	// Remove used UTXOs
	for (const auto& tx : block.txs) {
		for (const auto& input : tx.txInputs) {
			deleteUtxo(*utxoDb, input);
		}
	}
}

void undoBlock() {

	auto tipHash = getBlockchainTip();

	// Open undo file for reading
	fs::path undoFilePath = undoPath / (bytesToHex(tipHash) + ".undo");
	auto undoDataBytes = readWholeFile(undoFilePath);
	fs::path blockFilePath = blocksPath / (bytesToHex(tipHash) + ".block");
	auto blockBytes = readWholeFile(blockFilePath);
	auto block = formatBlock(blockBytes);

	auto utxoDb = openUtxoDb();

	// Restore previous blockchain tip
	removeBlockchainTip();

	// Remove created UTXOs
	for (const auto& tx : block.txs) {
		const auto txHash = getTxHash(tx);
		for (uint32_t i = 0; i < tx.txOutputs.size(); i++) {
			deleteUtxo(*utxoDb, { txHash, i });
		}
	}

	// Restore used UTXOs
	size_t offset = 0;
	for (uint32_t i = 0; i < undoDataBytes.size();) {
		Tx tx;
		// Read transaction version
		takeBytesInto(tx.version, undoDataBytes, offset);
		// Read input count
		size_t inputCount;
		takeBytesInto(inputCount, undoDataBytes, offset);
		// Read each input
		for (size_t j = 0; j < inputCount; j++) {
			TxInput input;
			// Read UTXO reference
			takeBytesInto(input.UTXOTxHash, undoDataBytes, offset);
			takeBytesInto(input.UTXOOutputIndex, undoDataBytes, offset);
			// Read UTXO value
			TxOutput utxo;
			takeBytesInto(utxo.amount, undoDataBytes, offset);
			takeBytesInto(utxo.recipient, undoDataBytes, offset);
			// Restore UTXO
			putUtxo(*utxoDb, input, utxo);
		}
	}
	// Delete block file
	fs::remove(blockFilePath);
	// Delete undo file
	fs::remove(undoFilePath);
}

bool blockExists(const Array256_t& blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	return fs::exists(blockFilePath);
}

static Block getGenesisBlock() {
	// ================== Genesis Tx ==================
	TxOutput genesisOutput;
	genesisOutput.amount = 50;
	genesisOutput.recipient = {};
	// Example recipient public key (32 bytes of zeros for simplicity)

	Tx genesisTx;
	genesisTx.version = 1;
	genesisTx.txInputs = {}; // No inputs for genesis tx
	genesisTx.txOutputs = { genesisOutput };

	// ================== Genesis Block ==================

	Block genesisBlock;
	genesisBlock.header.merkleRoot = getTxHash(genesisTx);
	genesisBlock.txs = { genesisTx };


	return genesisBlock;
}

Array256_t getGenesisBlockHash() {
	return getBlockHash(getGenesisBlock());
}


// ===========================================================
// UTXO storage management
// ===========================================================

static std::unique_ptr<leveldb::DB> openUtxoDb() {
	fs::create_directories(utxoPath);

	leveldb::Options options;
	options.create_if_missing = true;

	leveldb::DB* raw = nullptr;
	leveldb::Status status = leveldb::DB::Open(options, (utxoPath / "leveldb").string(), &raw);

	if (!status.ok() || !raw)
		throw std::runtime_error("Failed to open LevelDB: " + status.ToString());

	return std::unique_ptr<leveldb::DB>(raw);
}

static void putUtxo(leveldb::DB& db, const TxInput& txInput, const TxOutput& utxo) {
	std::string keyString;
	appendBytes(keyString, txInput.UTXOTxHash);
	appendBytes(keyString, txInput.UTXOOutputIndex);

	std::string valueString;
	appendBytes(valueString, utxo.amount);
	appendBytes(valueString, utxo.recipient);

	leveldb::Slice key(keyString);
	leveldb::Slice value(valueString);

	leveldb::Status status = db.Put(leveldb::WriteOptions(), key, value);
	if (!status.ok())
		throw std::runtime_error("Failed to put UTXO: " + status.ToString());
}

static void deleteUtxo(leveldb::DB& db, const TxInput& txInput) {
	// Construct key
	std::string keyString;
	appendBytes(keyString, txInput.UTXOTxHash);
	appendBytes(keyString, txInput.UTXOOutputIndex);

	leveldb::Slice key(keyString);

	// Delete UTXO
	leveldb::Status status = db.Delete(leveldb::WriteOptions(), key);
	if (!status.ok())
		throw std::runtime_error("Failed to delete UTXO: " + status.ToString());
}

static TxOutput getUtxoValue(leveldb::DB& db, const TxInput& txInput) {
	// Construct key
	std::string keyString;
	appendBytes(keyString, txInput.UTXOTxHash);
	appendBytes(keyString, txInput.UTXOOutputIndex);

	leveldb::Slice key(keyString);

	// Fetch raw value
	std::string value;
	leveldb::Status status = db.Get(leveldb::ReadOptions(), key, &value);

	if (!status.ok())
		throw std::runtime_error("UTXO not found");

	// Decode bytes
	TxOutput utxo;
	size_t offset = 0;

	takeBytesInto(
		utxo.amount,
		{ reinterpret_cast<const uint8_t*>(value.data()), value.size() },
		offset
	);

	takeBytesInto(
		utxo.recipient,
		{ reinterpret_cast<const uint8_t*>(value.data()), value.size() },
		offset
	);

	return utxo;
}

static bool utxoValid(leveldb::DB* db, const TxInput& txInput) {

	if (db == nullptr) {
		throw std::runtime_error("UTXO DB pointer is null");
	}

	// Construct key
	std::string keyString;
	appendBytes(keyString, txInput.UTXOOutputIndex);
	appendBytes(keyString, txInput.UTXOTxHash);
	leveldb::Slice key(keyString);

	// Query DB
	std::string value;
	leveldb::Status status = db->Get(leveldb::ReadOptions(), key, &value);

	// If the key exists, ok == true
	return status.ok();
}

// ===========================================================
// Blockchain tip management
// ===========================================================

// Add a new tip to the end of the file
void addBlockchainTip(const Array256_t& newTip) {
	auto file = openFileForAppend(blockHashesFilePath);
	appendToFile(file, newTip); // write 32 bytes of hash
}

void removeBlockchainTip() {
	if (!fs::exists(blockHashesFilePath)) return;

	uintmax_t size = fs::file_size(blockHashesFilePath);
	if (size < sizeof(Array256_t))
		throw std::runtime_error("Cannot remove tip: file too small.");

	fs::resize_file(blockHashesFilePath, size - sizeof(Array256_t));
}
// Get the last tip (the tip is the last 32 bytes of the file)
Array256_t getBlockchainTip() {
	if (!fs::exists(blockHashesFilePath))
		throw std::runtime_error("Blockchain tip file does not exist.");

	std::ifstream file(blockHashesFilePath, std::ios::binary | std::ios::ate);
	if (!file)
		throw std::runtime_error("Failed to open blockchain tip file.");

	std::size_t fileSize = static_cast<std::size_t>(file.tellg());
	if (fileSize < sizeof(Array256_t))
		throw std::runtime_error("Blockchain tip file is empty or corrupted.");

	file.seekg(fileSize - sizeof(Array256_t), std::ios::beg);

	Array256_t tip;
	file.read(reinterpret_cast<char*>(tip.data()), sizeof(Array256_t));
	if (!file)
		throw std::runtime_error("Failed to read blockchain tip.");

	return tip;
}


// ===========================================================
// Peer address storage management
// ===========================================================

void storePeers(std::unordered_map<PeerAddress, PeerStatus>& peers) {
	fs::create_directories(peersFilePath.parent_path());
	std::ofstream peersFile(peersFilePath, std::ios::trunc | std::ios::binary);
	peersFile.exceptions(std::ios::failbit | std::ios::badbit);

	// Peers count
	appendToFile(peersFile, static_cast<uint64_t>(peers.size()));

	for (const auto& [peerAddr, peerStatus] : peers) {
		// Address length
		const size_t addrLen = peerAddr.address.size();
		appendToFile(peersFile, static_cast<uint16_t>(addrLen));
		// Write address string
		appendToFile(peersFile, peerAddr.address);
		// Write port
		appendToFile(peersFile, peerAddr.port);
		// Write services
		appendToFile(peersFile, peerStatus.services);
		// Write last seen
		appendToFile(peersFile, peerStatus.lastSeen);
	}
}

std::unordered_map<PeerAddress, PeerStatus> loadPeers() {
	std::unordered_map<PeerAddress, PeerStatus> peers;

	auto peersFileBytes = readWholeFile(peersFilePath);
	size_t offset = 0;

	// Read peers count
	uint64_t peersCount = 0;
	takeBytesInto(peersCount, peersFileBytes, offset);

	for (uint64_t i = 0; i < peersCount; i++) {
		PeerAddress peerAddr;
		PeerStatus peerStatus;
		// Read address length
		uint16_t addrLen = 0;
		takeBytesInto(addrLen, peersFileBytes, offset); // assumes it converts from LE to host order

		// Read address string
		std::string addr(addrLen, '\0');
		takeBytesInto(addr, peersFileBytes, offset);

		// Read port
		takeBytesInto(peerAddr.port, peersFileBytes, offset);

		// Read services
		takeBytesInto(peerStatus.services, peersFileBytes, offset);

		// Read lastSeen
		takeBytesInto(peerStatus.lastSeen, peersFileBytes, offset);

		// Insert into map
		peers.insert({ peerAddr, peerStatus });

	}
	return peers;
}




