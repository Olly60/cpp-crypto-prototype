#include <filesystem>
#include <fstream>
#include "leveldb/db.h"
#include "crypto_utils.h"
#include "storage.h"
#include <algorithm>
#include <unordered_map>
#include "network.h"

namespace fs = std::filesystem;

// ============================================================================
// FILE PATHS
// ============================================================================

namespace paths {
	const fs::path blockchain = "blockchain";
	const fs::path blockchainTip = blockchain / "blockchain_tip" / "blockchain_tip";
	const fs::path blocks = blockchain / "blocks";
	const fs::path utxo = blockchain / "utxo";
	const fs::path undo = blockchain / "undo";
	const fs::path peers = blockchain / "peers" / "peers_list";
	const fs::path blockHeight = blockchain / "blockchain_height" / "height";
	const fs::path heightDb = blockchain / "block_heights" / "heights";
}

// ============================================================================
// FILE I/O UTILITIES
// ============================================================================

namespace {
	std::ofstream openFileForAppend(const fs::path& path) {
		try {
			fs::create_directories(path.parent_path());
			std::ofstream file(path, std::ios::app | std::ios::binary);
			if (!file) {
				throw std::runtime_error("Failed to open file for append");
			}
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

	std::vector<uint8_t> readWholeFile(const fs::path& filePath) {
		if (!fs::exists(filePath)) {
			throw std::runtime_error("File does not exist: " + filePath.string());
		}

		std::ifstream file;
		file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

		try {
			file.open(filePath, std::ios::binary | std::ios::ate);

			std::streamsize size = file.tellg();
			if (size < 0) {
				throw std::runtime_error("Failed to determine file size: " + filePath.string());
			}

			file.seekg(0, std::ios::beg);

			std::vector<uint8_t> buffer(static_cast<size_t>(size));
			if (size > 0) {
				file.read(reinterpret_cast<char*>(buffer.data()), size);
			}

			return buffer;
		}
		catch (const std::ios_base::failure& e) {
			throw std::runtime_error("Failed to read file " + filePath.string() + ": " + e.what());
		}
	}

	fs::path getBlockFilePath(const Array256_t& blockHash) {
		return paths::blocks / (bytesToHex(blockHash) + ".block");
	}

	fs::path getUndoFilePath(const Array256_t& blockHash) {
		return paths::undo / (bytesToHex(blockHash) + ".undo");
	}

}

// ============================================================================
// BLOCK FILE OPERATIONS
// ============================================================================

std::vector<uint8_t> readBlockFile(const Array256_t& blockHash) {
	return readWholeFile(getBlockFilePath(blockHash));
}

std::vector<uint8_t> readBlockFileHeader(const Array256_t& blockHash) {
	auto blockBytes = readBlockFile(blockHash);

	constexpr size_t headerSize = calculateBlockHeaderSize();
	if (blockBytes.size() < headerSize) {
		throw std::runtime_error("Block file too small to contain header");
	}

	return std::vector<uint8_t>(blockBytes.begin(), blockBytes.begin() + headerSize);
}

// ============================================================================
// READ INFOMATION
// ============================================================================

bool blockExists(const Array256_t& blockHash) {
	return fs::exists(getBlockFilePath(blockHash));
}

Block getBlockByHash(const Array256_t& blockHash) {
	return formatBlock(readBlockFile(blockHash));
}

BlockHeader getBlockHeaderByHash(const Array256_t& blockHash) {
	return formatBlockHeader(readBlockFileHeader(blockHash));
}

BlockHeader getBlockHeaderByHeight(const uint64_t& height) {

	return formatBlockHeader(readBlockFileHeader(height))
}

// ============================================================================
// UTXO DATABASE OPERATIONS
// ============================================================================

namespace {

	std::string makeUtxoKey(const TxInput& txInput) {
		std::string key;
		appendBytes(key, txInput.UTXOTxHash);
		appendBytes(key, txInput.UTXOOutputIndex);
		return key;
	}

	std::string makeUtxoValue(const TxOutput& utxo) {
		std::string value;
		appendBytes(value, utxo.amount);
		appendBytes(value, utxo.recipient);
		return value;
	}

	TxOutput formatUtxoValue(const std::string& value) {
		TxOutput utxo;
		size_t offset = 0;
		std::span<const uint8_t> data(
			reinterpret_cast<const uint8_t*>(value.data()),
			value.size()
		);
		takeBytesInto(utxo.amount, data, offset);
		takeBytesInto(utxo.recipient, data, offset);
		return utxo;
	}

}

void putUtxo(leveldb::DB& db, const TxInput& txInput, const TxOutput& utxo) {
	std::string key = makeUtxoKey(txInput);
	std::string value = makeUtxoValue(utxo);

	leveldb::Status status = db.Put(
		leveldb::WriteOptions(),
		leveldb::Slice(key),
		leveldb::Slice(value)
	);

	if (!status.ok()) {
		throw std::runtime_error("Failed to put UTXO: " + status.ToString());
	}
}

void deleteUtxo(leveldb::DB& db, const TxInput& txInput) {
	std::string key = makeUtxoKey(txInput);

	leveldb::Status status = db.Delete(
		leveldb::WriteOptions(),
		leveldb::Slice(key)
	);

	if (!status.ok()) {
		throw std::runtime_error("Failed to delete UTXO: " + status.ToString());
	}
}

TxOutput getUtxo(leveldb::DB& db, const TxInput& txInput) {
	std::string key = makeUtxoKey(txInput);
	std::string value;

	leveldb::Status status = db.Get(
		leveldb::ReadOptions(),
		leveldb::Slice(key),
		&value
	);

	if (!status.ok()) {
		throw std::runtime_error("UTXO not found: " + status.ToString());
	}

	return formatUtxoValue(value);
}

std::unique_ptr<leveldb::DB> openUtxoDb() {
	fs::create_directories(paths::utxo);

	leveldb::Options options;
	options.create_if_missing = true;

	leveldb::DB* raw = nullptr;
	leveldb::Status status = leveldb::DB::Open(
		options,
		(paths::utxo / "leveldb").string(),
		&raw
	);

	if (!status.ok() || !raw) {
		throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
	}

	return std::unique_ptr<leveldb::DB>(raw);
}

bool utxoInDb(leveldb::DB& db, const TxInput& txInput) {
	std::string key = makeUtxoKey(txInput);
	std::string value;

	leveldb::Status status = db.Get(
		leveldb::ReadOptions(),
		leveldb::Slice(key),
		&value
	);

	return status.ok();
}

// ============================================================================
// UNDO DATA MANAGEMENT
// ============================================================================

namespace {
	struct UndoData {
		std::vector<Tx> transactions;
		std::vector<std::vector<TxOutput>> spentUtxos;
	};

	void writeUndoFile(const fs::path& undoFilePath,
		const Block& block,
		leveldb::DB& utxoDb) {
		auto undoFile = openFileForAppend(undoFilePath);

		for (const auto& tx : block.txs) {
			// Write transaction version
			appendToFile(undoFile, tx.version);

			// Write input count
			appendToFile(undoFile, static_cast<uint64_t>(tx.txInputs.size()));

			// For each input, write UTXO reference and value
			for (const auto& input : tx.txInputs) {
				appendToFile(undoFile, input.UTXOTxHash);
				appendToFile(undoFile, input.UTXOOutputIndex);

				// Retrieve and write UTXO value
				TxOutput usedUtxo = getUtxo(utxoDb, input);
				appendToFile(undoFile, usedUtxo.amount);
				appendToFile(undoFile, usedUtxo.recipient);
			}
		}
	}

	void restoreFromUndoFile(const fs::path& undoFilePath, leveldb::DB& utxoDb) {
		auto undoDataBytes = readWholeFile(undoFilePath);
		size_t offset = 0;

		while (offset < undoDataBytes.size()) {
			// Read transaction version
			uint64_t version;
			takeBytesInto(version, undoDataBytes, offset);

			// Read input count
			uint64_t inputCount;
			takeBytesInto(inputCount, undoDataBytes, offset);

			// Read each input and restore UTXO
			for (uint64_t j = 0; j < inputCount; j++) {
				TxInput input;
				takeBytesInto(input.UTXOTxHash, undoDataBytes, offset);
				takeBytesInto(input.UTXOOutputIndex, undoDataBytes, offset);

				TxOutput utxo;
				takeBytesInto(utxo.amount, undoDataBytes, offset);
				takeBytesInto(utxo.recipient, undoDataBytes, offset);

				// Restore UTXO
				putUtxo(utxoDb, input, utxo);
			}
		}
	}
}

// ============================================================================
// BLOCK STORAGE MANAGEMENT
// ============================================================================

void addBlock(const Block& block) {
	const Array256_t blockHash = getBlockHash(block);
	const fs::path blockFilePath = getBlockFilePath(blockHash);
	const fs::path undoFilePath = getUndoFilePath(blockHash);

	// Open UTXO database
	auto utxoDb = openUtxoDb();

	// Write undo data before modifying UTXO set
	writeUndoFile(undoFilePath, block, *utxoDb);

	// Write block to disk
	auto blockFile = openFileForAppend(blockFilePath);
	const auto blockBytes = serialiseBlock(block);
	blockFile.write(
		reinterpret_cast<const char*>(blockBytes.data()),
		blockBytes.size()
	);

	// Update UTXO set: add new UTXOs
	for (const auto& tx : block.txs) {
		const Array256_t txHash = getTxHash(tx);
		for (uint64_t outputIndex = 0; outputIndex < tx.txOutputs.size(); outputIndex++) {
			TxInput utxoKey{ txHash, outputIndex, {} };
			putUtxo(*utxoDb, utxoKey, tx.txOutputs[outputIndex]);
		}
	}

	// Update UTXO set: remove spent UTXOs
	for (const auto& tx : block.txs) {
		for (const auto& input : tx.txInputs) {
			deleteUtxo(*utxoDb, input);
		}
	}

	// Update blockchain tip
	setBlockchainTip(blockHash);

	// Update block height
	addBlockHeight();
}

void undoBlock() {
	const Array256_t tipHash = getBlockchainTip();
	const fs::path blockFilePath = getBlockFilePath(tipHash);
	const fs::path undoFilePath = getUndoFilePath(tipHash);

	// Read block
	Block block = getBlock(tipHash);

	// Open UTXO database
	auto utxoDb = openUtxoDb();

	// Remove created UTXOs
	for (const auto& tx : block.txs) {
		const Array256_t txHash = getTxHash(tx);
		for (uint64_t i = 0; i < tx.txOutputs.size(); i++) {
			TxInput utxoKey{ txHash, i, {} };
			deleteUtxo(*utxoDb, utxoKey);
		}
	}

	// Restore spent UTXOs from undo file
	restoreFromUndoFile(undoFilePath, *utxoDb);

	// Update blockchain tip
	setBlockchainTip(block.header.prevBlockHash);

	// Update block height
	subtractBlockHeight();

	// Delete files
	fs::remove(blockFilePath);
	fs::remove(undoFilePath);
}

// ============================================================================
// GENESIS BLOCK
// ============================================================================

Block getGenesisBlock() {
	// Genesis transaction
	TxOutput genesisOutput{
		0,         // amount
		{}          // recipient (empty for genesis)
	};

	Tx genesisTx{
		1,          // version
		{},         // no inputs
		{genesisOutput}
	};

	// Genesis block
	BlockHeader header;
	header.merkleRoot = getTxHash(genesisTx);

	Block genesisBlock{
		header,
		{genesisTx}
	};

	return genesisBlock;
}

Array256_t getGenesisBlockHash() {
	return getBlockHash(getGenesisBlock());
}

// ============================================================================
// BLOCKCHAIN TIP MANAGEMENT
// ============================================================================

void setBlockchainTip(const Array256_t& newTip) {
	fs::create_directories(paths::blockchainTip.parent_path());

	std::ofstream blockchainTipFile(paths::blockchainTip, std::ios::trunc | std::ios::binary);
	if (!blockchainTipFile) {
		throw std::runtime_error("Failed to open blockchain tip file for writing");
	}

	appendToFile(blockchainTipFile, newTip);
}

Array256_t getBlockchainTip() {
	if (!fs::exists(paths::blockchainTip)) {
		throw std::runtime_error("Blockchain tip file does not exist");
	}

	std::ifstream file(paths::blockchainTip, std::ios::binary | std::ios::ate);
	if (!file) {
		throw std::runtime_error("Failed to open blockchain tip file");
	}

	std::streamsize fileSize = file.tellg();
	if (fileSize < static_cast<std::streamsize>(sizeof(Array256_t))) {
		throw std::runtime_error("Blockchain tip file is empty or corrupted");
	}

	file.seekg(fileSize - static_cast<std::streamsize>(sizeof(Array256_t)), std::ios::beg);

	Array256_t tip;
	file.read(reinterpret_cast<char*>(tip.data()), sizeof(Array256_t));
	if (!file) {
		throw std::runtime_error("Failed to read blockchain tip");
	}

	return tip;
}

// ============================================================================
// PEER STORAGE MANAGEMENT
// ============================================================================

void storePeers(const std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash>& peers) {
	fs::create_directories(paths::peers.parent_path());

	std::ofstream peersFile(paths::peers, std::ios::trunc | std::ios::binary);
	if (!peersFile) {
		throw std::runtime_error("Failed to open peers file for writing");
	}
	peersFile.exceptions(std::ios::failbit | std::ios::badbit);

	// Write peer count
	appendToFile(peersFile, static_cast<uint64_t>(peers.size()));

	// Write each peer
	for (const auto& [peerAddr, peerStatus] : peers) {
		// Address length and string
		appendToFile(peersFile, static_cast<uint16_t>(peerAddr.address.size()));
		peersFile.write(peerAddr.address.data(), peerAddr.address.size());

		// Port
		appendToFile(peersFile, peerAddr.port);

		// Services
		appendToFile(peersFile, peerStatus.services);

		// Last seen
		appendToFile(peersFile, peerStatus.lastSeen);
	}
}

std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> loadPeers() {
	if (!fs::exists(paths::peers)) {
		return {};
	}

	std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> peers;
	auto peersFileBytes = readWholeFile(paths::peers);
	size_t offset = 0;

	// Read peer count
	uint64_t peersCount;
	takeBytesInto(peersCount, peersFileBytes, offset);

	// Read each peer
	for (uint64_t i = 0; i < peersCount; i++) {
		PeerAddress peerAddr;
		PeerStatus peerStatus;

		// Read address length
		uint16_t addrLen;
		takeBytesInto(addrLen, peersFileBytes, offset);

		// Read address string
		if (offset + addrLen > peersFileBytes.size()) {
			throw std::runtime_error("Peers file corrupted: address exceeds file size");
		}
		peerAddr.address = std::string(
			reinterpret_cast<const char*>(peersFileBytes.data() + offset),
			addrLen
		);
		offset += addrLen;

		// Read port
		takeBytesInto(peerAddr.port, peersFileBytes, offset);

		// Read services
		takeBytesInto(peerStatus.services, peersFileBytes, offset);

		// Read last seen
		takeBytesInto(peerStatus.lastSeen, peersFileBytes, offset);

		peers.insert({ peerAddr, peerStatus });
	}

	return peers;
}

// ============================================================================
// BLOCK HEIGHT MANAGEMENT
// ============================================================================

void addBlockHeight() {
	fs::create_directories(paths::blockHeight.parent_path());

	std::ofstream heightFile(paths::blockHeight, std::ios::trunc | std::ios::binary);
	if (!heightFile) {
		throw std::runtime_error("Failed to open block height file for writing");
	}
	uint64_t height = 0;
	takeBytesInto(height, readWholeFile(paths::blockHeight));
	heightFile.exceptions(std::ios::failbit | std::ios::badbit);
	appendToFile(heightFile, height + 1);

}

void subtractBlockHeight() {
	fs::create_directories(paths::blockHeight.parent_path());

	std::ofstream heightFile(paths::blockHeight, std::ios::trunc | std::ios::binary);
	readWholeFile(paths::blockHeight);
	uint64_t height = 0;
	takeBytesInto(height, readWholeFile(paths::blockHeight));
	if (!heightFile) {
		throw std::runtime_error("Failed to open peers block height for writing");
	}
	heightFile.exceptions(std::ios::failbit | std::ios::badbit);
	if (!height - 1 > height) {
		appendToFile(heightFile, height - 1);
	}
	else {
		throw std::runtime_error("Block height is already 0");
	}

}

uint64_t getBlockHeight() {
	uint64_t height = 0;
	takeBytesInto(height, readWholeFile(paths::blockHeight));
	return height;
}

// ============================================================================
// KEY:HEIGHT VALUE:HASH
// ============================================================================

namespace {

	std::string makeHeightKey(const uint64_t& height) {
		std::string key;
		appendBytes(key, height);
		return key;
	}

	std::string makeHashValue(const Array256_t& hash) {
		std::string value;
		appendBytes(value, hash);
		return value;
	}

	Array256_t formatHashValue(const std::string& value) {
		Array256_t hash;
		std::span<const uint8_t> data(
			reinterpret_cast<const uint8_t*>(value.data()),
			value.size()
		);
		takeBytesInto(hash, std::span<const uint8_t>(
			reinterpret_cast<const uint8_t*>(value.data()),
			value.size()
		));
		return hash;
	}

}

void putHeightHash(leveldb::DB& db, const uint64_t& height, const Array256_t& hash) {
	std::string key = makeHeightKey(height);
	std::string value = makeHashValue(hash);

	leveldb::Status status = db.Put(
		leveldb::WriteOptions(),
		leveldb::Slice(key),
		leveldb::Slice(value)
	);

	if (!status.ok()) {
		throw std::runtime_error("Failed to put height: " + status.ToString());
	}
}

void deleteHeightHash(leveldb::DB& db, const uint64_t& height) {
	std::string key = makeHeightKey(height);

	leveldb::Status status = db.Delete(
		leveldb::WriteOptions(),
		leveldb::Slice(key)
	);

	if (!status.ok()) {
		throw std::runtime_error("Failed to delete height: " + status.ToString());
	}
}

TxOutput getHeightHash(leveldb::DB& db, const uint64_t& height) {
	std::string key = makeHeightKey(height);
	std::string value;

	leveldb::Status status = db.Get(
		leveldb::ReadOptions(),
		leveldb::Slice(key),
		&value
	);

	if (!status.ok()) {
		throw std::runtime_error("UTXO not found: " + status.ToString());
	}

	return formatUtxoValue(value);
}

std::unique_ptr<leveldb::DB> openHeightHash() {
	fs::create_directories(paths::heightDb);

	leveldb::Options options;
	options.create_if_missing = true;

	leveldb::DB* raw = nullptr;
	leveldb::Status status = leveldb::DB::Open(
		options,
		(paths::utxo / "leveldb").string(),
		&raw
	);

	if (!status.ok() || !raw) {
		throw std::runtime_error("Failed to open LevelDB: " + status.ToString());
	}

	return std::unique_ptr<leveldb::DB>(raw);
}

bool HeightHashInDb(leveldb::DB& db, const TxInput& txInput) {
	std::string key = makeUtxoKey(txInput);
	std::string value;

	leveldb::Status status = db.Get(
		leveldb::ReadOptions(),
		leveldb::Slice(key),
		&value
	);

	return status.ok();
}