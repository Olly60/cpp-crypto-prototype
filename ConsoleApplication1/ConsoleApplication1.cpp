#include <sodium.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
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

static void bytesFromHex(hash256_t& out, string hex) {
	for (size_t i = 0; i < hex.size(); i = i + 2) {
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

static void hexFromBytes(string& out, hash256_t bytes, size_t size) {
	out.clear();
	out.resize(size * 2);
	for (size_t i = 0; i < size; i++) {
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

static void sha256Of(hash256_t& out, const void* data, size_t len) {
	crypto_hash_sha256(out.data(), reinterpret_cast<const uint8_t*>(data), len);
}

static bool verifyBlock(const Block& block) {
	// Version 1 block verification
	if (block.header.version == 1) {

		// Invalid block hash
		hash256_t blockHash;
		sha256Of(blockHash, &block.header, sizeof(BlockHeader));

		// Verify block header
		// Already in chain
		if (blockChain.count(block.blockHash) == 1) return false;
		// Previous block not found
		if (blockChain.count(block.header.previousBlockHash) == 0) return false;

		// Verify each transaction
		vector<UTXOKey> seenUtxo;
		hash256_t txHash;
		uint64_t blockReward = 0;
		bool isCoinbaseTx = true;
		for (const Transaction& tx : block.transactions) {

			// Coinbase transaction
			if (isCoinbaseTx) { isCoinbaseTx = false; continue; }

			// Transaction hash
			hash256_t inOutHashes;
			tx.txInputs
			sha256Of(inOutHashes[0], );
			sha256Of(inOutHashes[0], tx.txInputs.data(), tx.txInputs.size() * sizeof(UTXO));
			sha256Of(inOutHashes[1], tx.txOutputs.data(), tx.txOutputs.size() * sizeof(UTXO));
			sha256Of(txHash, inOutHashes.data(), inOutHashes.size() * sizeof(hash256_t));

			// Verify each input
			UTXOKey key;
			uint64_t totalInputAmount = 0;
			uint64_t totalOutputAmount = 0;
			for (const TxInputSigned& txInputSigned : tx.txInputs) {

				key.txHash = txInputSigned.txInput.txHash;
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
		if (coinabaseAmount != BlockReward) return false;



		// All checks passed
		// Add block to chain
		blockChain[block.blockHash] = block;

		// Remove used UTXOs
		for (const Transaction& tx : block.transactions) {
			for (TxInputSigned txInputSigned : tx.txInputs) {
				UTXOKey key;
				key.txHash = txInputSigned.txInput.txHash;
				key.outputIndex = txInputSigned.txInput.outputIndex;
				UTXOs.erase(key);
			}
		}

		// Add new UTXOs from Transactions
		for (const Transaction& tx : block.transactions) {
			// Transaction hash
			hash256_t inOutHashes;
			sha256Of(inOutHashes[0], tx.txInputs.data(), tx.txInputs.size() * sizeof(UTXO));
			sha256Of(inOutHashes[1], tx.txOutputs.data(), tx.txOutputs.size() * sizeof(UTXO));
			sha256Of(txHash, inOutHashes.data(), inOutHashes.size() * sizeof(hash256_t));

			// Add each output as UTXO
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
		const Transaction& coinbaseTx = block.transactions[0];
		hash256_t inOutHashes;
		sha256Of(inOutHashes[0], coinbaseTx.txInputs.data(), coinbaseTx.txInputs.size() * sizeof(UTXO));
		sha256Of(inOutHashes[1], coinbaseTx.txOutputs.data(), coinbaseTx.txOutputs.size() * sizeof(UTXO));
		sha256Of(txHash, inOutHashes.data(), inOutHashes.size() * sizeof(hash256_t));
		UTXOKey key;
		size_t index = 0;
		for (const UTXO& coinbaseTxOutput : coinbaseTx.txOutputs) {
			key.txHash = txHash;
			key.outputIndex = index;
			UTXOs[key] = coinbaseTxOutput;
			index++;
		}

		// Block is valid
		return true;
	}
}

// Unspent Transaction Output
struct UTXO {
	uint64_t amount;
	hash256_t recipient;
};

// Transaction Input Decides which UTXO to Spend
struct TxInput {
	hash256_t txHash;
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
	time_t timestamp;
	uint64_t nonce;
	uint64_t difficulty;
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