#include <sodium.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <fstream>
typedef array<uint8_t, 32> hash256_t;
using namespace std;
array<uint8_t, crypto_box_SECRETKEYBYTES> bytesPrivateKey;
array<uint8_t, crypto_box_PUBLICKEYBYTES> bytesPublicKey;
string hexPrivateKey;
string hexPublicKey;
array<uint8_t, crypto_sign_BYTES> signature;
string userInput;
unordered_map<UTXOKey, UTXO, UTXOKeyHash> UTXOs;
unordered_map<array<uint8_t, 32>, Block> blockChain;

// Custom hash function for UTXOKey
struct UTXOKeyHash {
	size_t operator()(const UTXOKey& key) const noexcept {
		const uint64_t* chunks = reinterpret_cast<const uint64_t*>(key.txHash.data());
		size_t data = chunks[0] ^ chunks[1] ^ chunks[2] ^ chunks[3];
		data ^= key.outputIndex + 0x9e3779b97f4a7c15ULL + (data << 6) + (data >> 2);
		return data;
	}
};

// Unspent Transaction Output Key
struct UTXOKey {
	hash256_t txHash;
	uint64_t outputIndex;
};

// Convert hexadecimal string to byte array
static void bytesFromHex(hash256_t& out, const string &hex) {
	for (uint64_t i = 0; i < hex.size(); i = i + 2) {
		uint8_t high = toupper(hex[i]);
		uint8_t low = toupper(hex[i + 1]);
		if (high >= '0' && high <= '9') {
			high = high - '0';
		}
		else if (high >= 'A' && high <= 'F') {
			high = (high - 'A') + 10;
		}
		if (low >= '0' && low <= '9') {
			low = low - '0';
		}
		else if (low >= 'A' && low <= 'F') {
			low = (low - 'A') + 10;
		}
		out[i / 2] = (high << 4) | low;
	}
}

// Convert byte array to hexadecimal string
static void hexFromBytes(string& out, const hash256_t &bytes, const uint64_t &size) {
	out.clear();
	out.resize(size * 2);
	for (uint64_t i = 0; i < size; i++) {
		uint8_t high = bytes[i] >> 4;
		uint8_t low = bytes[i] & 0x0F;

		if (high >= 0 && high <= 9) {
			high += '0';
		}
		else if (high >= 10) high += ('A' - 10);
		if (low >= 0 && low <= 9) {
			low += '0';
		}
		else if (low >= 10) low += ('A' - 10);
		out[i * 2] = high;
		out[(i * 2) + 1] = low;
	}
}

// SHA-256 Hashing
static void sha256Of(hash256_t& out, const void* data, const uint64_t &len) {
	crypto_hash_sha256(out.data(), reinterpret_cast<const uint8_t*>(data), len);
}

// Little-endian uint64_t to byte array
static void putUint64LE(uint8_t* buf, const uint64_t &value) {
	for (uint8_t i = 0; i < 8; i++) buf[i] = (value >> (i * 8)) & 0xFF;
}

// Sterilise transaction for hashing
static void hashTransaction(hash256_t& out, const Transaction& tx) {
	vector<uint8_t> inOutSerilised;
	array<uint8_t, 8> buffer;
	for (const TxInputSigned& txInputSigned : tx.txInputs) {
		putUint64LE(buffer.data(), txInputSigned.txInput.outputIndex);
		inOutSerilised.insert(inOutSerilised.end(), buffer.begin(), buffer.end());
		inOutSerilised.insert(inOutSerilised.end(), txInputSigned.txInput.prevTxHash.begin(), txInputSigned.txInput.prevTxHash.end());
		for (const UTXO& txOutput : tx.txOutputs) {
			putUint64LE(buffer.data(), txOutput.amount);
			inOutSerilised.insert(inOutSerilised.end(), buffer.begin(), buffer.end());
			inOutSerilised.insert(inOutSerilised.end(), txOutput.recipient.begin(), txOutput.recipient.end());
		}
		sha256Of(out, inOutSerilised.data(), inOutSerilised.size());
	}
}

static bool processBlock(const Block& block) {
	// Version 1 block verification
	if (block.header.version == 1) {

		// Calculate block hash
		hash256_t blockHash;
		array<uint8_t, 96> serializedHeader;
		putUint64LE(serializedHeader.data(), block.header.version);
		memcpy(serializedHeader.data() + 8, block.header.previousBlockHash.data(), 32);
		memcpy(serializedHeader.data() + 40, block.header.merkleRoot.data(), 32);
		putUint64LE(serializedHeader.data() + 72, block.header.timestamp);
		putUint64LE(serializedHeader.data() + 80, block.header.difficulty);
		putUint64LE(serializedHeader.data() + 88, block.header.nonce);
		sha256Of(blockHash, serializedHeader.data(), serializedHeader.size());

		// Verify block header
		// Already in chain
		if (blockChain.count(blockHash) == 1) return false;

		// Previous block not found
		if (blockChain.count(block.header.previousBlockHash) == 0) return false;

		// Timestamp is earlier than previous block
		if (block.header.timestamp < blockChain[block.header.previousBlockHash].header.timestamp) return false;

		// Timestamp is too far in the future
		uint64_t currentTime = time(0);
		if (block.header.timestamp > currentTime + (2 * (60 * 60))) return false;

		// Verify each transaction
		vector<UTXOKey> seenUtxo;
		uint64_t blockReward = 0;
		vector<uint8_t> txHashes;
		bool isCoinbaseTx = true;
		hash256_t txHash;
		vector<hash256_t> merkleLeaves;
		for (const Transaction& tx : block.transactions) {

			// Coinbase transaction
			if (isCoinbaseTx) { isCoinbaseTx = false; continue; }

			hashTransaction(txHash, tx);
			merkleLeaves.push_back(txHash);
			// Verify each input
			UTXOKey key;
			uint64_t totalInputAmount = 0;
			uint64_t totalOutputAmount = 0;
			for (const TxInputSigned& txInputSigned : tx.txInputs) {

				key.txHash = txInputSigned.txInput.prevTxHash;
				key.outputIndex = txInputSigned.txInput.outputIndex;

				// UTXO not found
				if (UTXOs.count(key) == 0) return false;

				// Invalid signature
				sha256Of(txHash, &txInputSigned.txInput, sizeof(TxInput));
				if (crypto_sign_verify_detached(txInputSigned.signature.data(), txHash.data(), 32, UTXOs[key].recipient.data()) != 0) return false;

				// Double spending
				if (find(seenUtxo.begin(), seenUtxo.end(), key) != seenUtxo.end()) return false;
				seenUtxo.push_back(key);

				// Calculate total input amount
				totalInputAmount += UTXOs[key].amount;
			}

			// Verify each output
			// Calculate total output amount
			for (UTXO txOutput : tx.txOutputs) {

				// Double spending of transaction
				if (UTXOs.count(key) != 0) return false;
				// Accumulate output amount
				totalOutputAmount += txOutput.amount;
			}
			// Output amount exceeds input amount subtract fee
			uint64_t txFee = max(totalInputAmount / 100, (uint64_t)1);
			if (totalOutputAmount > totalInputAmount - txFee) return false;

			// Accumulate total fees
			blockReward += txFee;
		}
		
		// Verify coinbase transaction
		// Coinbase transaction should have no inputs
		if (!block.transactions[0].txInputs.empty()) return false;

		// Coinbase transaction should have exactly one output
		if (block.transactions[0].txOutputs.size() != 1) return false;

		// Coinbase transaction output amount should equal total fees
		uint64_t coinabaseAmount = 0;
		for (const UTXO& coinbaseTxOutput : block.transactions[0].txOutputs) {
			coinabaseAmount += coinbaseTxOutput.amount;
		}
		// Coinbase amount exceeds total fees
		if (coinabaseAmount != blockReward) return false;

		// All checks passed
		// Add block to chain
		blockChain[blockHash] = block;

		// Remove used UTXOs
		for (const Transaction& tx : block.transactions) {
			for (TxInputSigned txInputSigned : tx.txInputs) {
				UTXOKey key;
				key.txHash = txInputSigned.txInput.prevTxHash;
				key.outputIndex = txInputSigned.txInput.outputIndex;
				UTXOs.erase(key);
			}
		}

		
		for (const Transaction& tx : block.transactions) {
			hashTransaction(txHash, tx);

			// Remove used input UTXO
			for (const TxInputSigned& txInputSigned : tx.txInputs) {
				UTXOKey key;
				key.txHash = txInputSigned.txInput.prevTxHash;
				key.outputIndex = txInputSigned.txInput.outputIndex;
				UTXOs.erase(key);
				
			}
			
			// Add new UTXOs from Transactions
			size_t index = 0;
			UTXOKey key;
			for (const UTXO& txOutput : tx.txOutputs) {
				key.txHash = txHash;
				key.outputIndex = index;
				UTXOs[key] = txOutput;
				index++;
			}
		}

		// Create UTXOs for Coinbase Transaction outputs
		hashTransaction(txHash, block.transactions[0]);
		UTXOKey key;
		uint64_t index = 0;
		for (const UTXO& coinbaseTxOutput : block.transactions[0].txOutputs) {
			key.txHash = txHash;
			key.outputIndex = index;
			UTXOs[key] = coinbaseTxOutput;
			index++;
		}

		// Block is valid
		return true;
	}

	// Unsupported block version
	return false;
}

// Unspent Transaction Output
struct UTXO {
	uint64_t amount;
	hash256_t recipient;
};

// Transaction Input Decides which UTXO to Spend
struct TxInput {
	hash256_t prevTxHash;
	uint64_t outputIndex;
};

// Signed Transaction Input
struct TxInputSigned {
	TxInput txInput;
	hash256_t signature;
};

// Transaction
struct Transaction {
	vector<TxInputSigned> txInputs;
	vector<UTXO> txOutputs;
};

// Block Header
struct BlockHeader {
	uint64_t version;
	hash256_t previousBlockHash;
	hash256_t merkleRoot;
	uint64_t timestamp;
	uint64_t difficulty;
	uint64_t nonce;
	
};

// Block
struct Block {
	BlockHeader header;
	vector<Transaction> transactions;
};



int main()
{
	cout << time(0);
	cout << "Enter your private key or type new to generate one: ";
	cin >> userInput;
	if (userInput == "new") {
		crypto_box_keypair(bytesPublicKey.data(), bytesPrivateKey.data());
		hexFromBytes(hexPublicKey, bytesPublicKey, crypto_box_PUBLICKEYBYTES);
		hexFromBytes(hexPrivateKey, bytesPrivateKey, crypto_box_SECRETKEYBYTES);
	}
	else {
		bytesFromHex(bytesPrivateKey, userInput);
		crypto_scalarmult_base(bytesPublicKey.data(), bytesPrivateKey.data());
		hexFromBytes(hexPublicKey, bytesPublicKey, crypto_box_PUBLICKEYBYTES);
		hexFromBytes(hexPrivateKey, bytesPrivateKey, crypto_box_SECRETKEYBYTES);
	}
	cout << "Public Key: " << hexPublicKey << endl << "Private Key: " << hexPrivateKey << endl;
}