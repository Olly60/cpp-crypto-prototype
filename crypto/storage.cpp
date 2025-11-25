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

// ==========================================================
// File I/O utilities
// ==========================================================

// Append bytes of data to output container
template <typename T>
void appendToFile(std::ofstream& file, const T& data) {

	// Inline little-endian serialization
	std::vector<uint8_t> temp(sizeof(T));
	appendBytes(temp, data);

	file.write(reinterpret_cast<const char*>(temp.data()), temp.size());
}

std::vector<uint8_t> readWholeFile(const fs::path& filePath) {
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
			auto usedUtxo = getUtxoValue(input);
			numIntoFile((usedUtxo.amount), undoFile);
			undoFile.write(reinterpret_cast<const char*>(usedUtxo.recipient.data()), usedUtxo.recipient.size());
		}

	}

	// Store UTXOs for all outputs in the block
	uint32_t outputIndex = 0;
	// Store new UTXOs
	for (const auto& tx : block.txs) {
		auto txHash = getTxHash(tx);
		for (const auto& UTXO : tx.txOutputs) {
			addUtxo({ txHash, outputIndex++ }, UTXO);
		}
	}
	// Remove used UTXOs
	for (const auto& tx : block.txs) {
		for (const auto& input : tx.txInputs) {
			removeUtxo(input);
		}
	}
}

static void undoBlock() {

	auto tipHash = getBlockchainTip();
	// Delete block file
	deleteBlockFile(tipHash);

	// Open undo file for reading
	fs::path undoFilePath = undoPath / (bytesToHex(tipHash) + ".undo");
	auto undoDataBytes = readWholeFile(undoFilePath);
	fs::path blockFilePath = blocksPath / (bytesToHex(tipHash) + ".block");
	auto blockBytes = readWholeFile(blockFilePath);
	auto block = formatBlock(blockBytes);

	// Remove created UTXOs
	for (const auto& tx : block.txs) {
		const auto txHash = getTxHash(tx);
		for (uint32_t i = 0; i < tx.txOutputs.size(); i++) {
			removeUtxo({ txHash, i });
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
			addUtxo(input, utxo);
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

static void deleteBlockFile(const Array256_t blockHash) {
	fs::path blockFilePath = blocksPath / (bytesToHex(blockHash) + ".block");
	if (fs::exists(blockFilePath)) fs::remove(blockFilePath);
}

// ===========================================================
// UTXO storage management
// ===========================================================

static void addUtxo(const TxInput& txInput, const TxOutput& utxo) {

	// Construct key
	std::string keyString;
	appendBytes(keyString, txInput.UTXOOutputIndex);
	appendBytes(keyString, txInput.UTXOTxHash);
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

static void removeUtxo(const TxInput& txInput) {
	// Construct key
	std::string keyString;
	appendBytes(keyString, txInput.UTXOOutputIndex);
	appendBytes(keyString, txInput.UTXOTxHash);
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

static TxOutput getUtxoValue(const TxInput &txInput) {

	// Construct key
	std::string keyString;
	appendBytes(keyString, txInput.UTXOOutputIndex);
	appendBytes(keyString, txInput.UTXOTxHash);
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

static bool utxoValid(const TxInput txInput) {
	// Construct key
	std::string keyString;
	appendBytes(keyString, txInput.UTXOOutputIndex);
	appendBytes(keyString, txInput.UTXOTxHash);
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

	auto tipFileBytes = readWholeFile(tipFilePath);

	if (tipFileBytes.size() != 32) {
		throw std::runtime_error("Blockchain tip file is not 32 bytes");
	}

	Array256_t latestBlockHash{};
	std::copy_n(tipFileBytes.begin(), 32, latestBlockHash.begin());

	return latestBlockHash;
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

static void deleteUndoFile(const Array256_t blockHash) {
	fs::path undoFilePath = undoPath / (bytesToHex(blockHash) + ".undo");
	if (fs::exists(undoFilePath)) fs::remove(undoFilePath);
}

// ===========================================================
// Peer address storage management
// ===========================================================

void storePeers(std::unordered_map<PeerAddress, PeerStatus> &peers) {
	fs::create_directories(peersPath);
	std::ofstream peersFile(peersFilePath, std::ios::trunc | std::ios::binary);

	peersFile.exceptions(std::ios::failbit | std::ios::badbit);
	try {
		for (const auto& [peerAddr, peerStatus] : peers) {
			// Address length
			const size_t addrLen = peerAddr.address.size();
			numIntoFile(static_cast<uint16_t>(addrLen), peersFile);
			// Write address string
			peersFile.write(peerAddr.address.data(), addrLen);
			// Write port
			numIntoFile(peerAddr.port, peersFile);
			// Write services
			numIntoFile(peerStatus.services, peersFile);
			// Write last seen
			numIntoFile(peerStatus.lastSeen, peersFile);
		}
	}
	catch (const std::ios_base::failure& e) {
		throw std::runtime_error("Failed to store peer address: " + std::string(e.what()));
	}
}

std::unordered_map<PeerAddress, PeerStatus> loadPeers() {
	std::unordered_map<PeerAddress, PeerStatus> peers;

	auto peersFileBytes = readWholeFile(peersFilePath);
	size_t offset = 0;

	// Read address length
	uint16_t addrLen = 0;
	takeBytesInto(addrLen, peersFileBytes, offset); // assumes it converts from LE to host order

			// Read address string
			std::string addr(addrLen, '\0');
			peersFile.read(addr.data(), addrLen);

			// Read port
			uint16_t port = 0;
			takeBytesInto(port, peersFileBytes, offset);

			// Read services
			uint64_t services = 0;
			takeBytesInto(services, peersFileBytes, offset);

			// Read lastSeen
			uint64_t lastSeen = 0;
			takeBytesInto(lastSeen, peersFileBytes, offset);

			// Insert into map
			PeerAddress peerAddr{ addr, port };
			PeerStatus peerStatus{ services, lastSeen };
			peers.emplace(std::move(peerAddr), std::move(peerStatus));

	return peers;
}




