#include <sodium.h>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
using namespace std;
array<uint8_t, crypto_box_SECRETKEYBYTES> bytesPrivateKey;
array<uint8_t, crypto_box_PUBLICKEYBYTES> bytesPublicKey;
string hexPrivateKey;
string hexPublicKey;
array<uint8_t, crypto_sign_BYTES> signature;
string userInput;
unordered_map<UTXOKey, UTXO> utxos;
unordered_map<array<uint8_t, 32>, Block> blockChain;


static void bytesFromHex(array<uint8_t, 32>& out, string hex) {
	for (size_t i = 0; i < hex.size(); i = i + 2) {
		uint8_t high = toupper(hex[i]);
		uint8_t low = toupper(hex[i + 1]);

		if (low >= '0' && low <= '9') {
			low = low - '0';
		}
		else if (low >= 'A' && low <= 'F') {
			low = (low - 'A') + 10;
		}
		if (high >= '0' && high <= '9') {
			high = high - '0';
		}
		else if (high >= 'A' && high <= 'F') {
			high = (high - 'A') + 10;
		}
		out[i / 2] = (low << 4) | high;
	}
}

static void hexFromBytes(string& out, array<uint8_t, 32> bytes, size_t size) {
	out.clear();
	out.resize(size * 2);
	for (size_t i = 0; i < size; i++) {
		uint8_t low = bytes[i] >> 4;
		uint8_t high = bytes[i] & 0x0F;

		if (low >= 0 && low <= 9) {
			low += '0';
		}
		else if (low >= 10 && low <= 16) {
			low += ('A' - 10);
		}
		if (high >= 0 && high <= 9) {
			high += '0';
		}
		else if (high >= 10 && high <= 16) {
			high += ('A' - 10);
		}
		out[i * 2] = high;
		out[(i * 2) + 1] = low;
	}
}

static bool verifyBlock(Block block) {
	// Version 1 block verification
	if (block.header.version = 1) {

		// Invalid block hash
		array<uint8_t, 32> blockHash;
		crypto_hash_sha256(blockHash.data(), (uint8_t*)&block.header, sizeof(BlockHeader));
		if (block.blockHash != blockHash) return false;

		// Verify block header
		// Already in chain
		if (blockChain.count(block.blockHash) == 1) return false;
		// Previous block not found
		if (blockChain.count(block.header.previousBlockHash) == 0) return false; 

		// Verify each transaction
		vector<UTXOKey> seenUtxo;
		array<uint8_t, 32> txHash;
		for (Transaction tx : block.transactions) {

			// Transaction hash
			array<array<uint8_t, 32>, 2> inOutHashes;
			crypto_hash_sha256(inOutHashes[0].data(), (uint8_t*)tx.txInputs.data(), tx.txOutputs.size() * sizeof(UTXO));
			crypto_hash_sha256(inOutHashes[1].data(), (uint8_t*)tx.txOutputs.data(), tx.txOutputs.size() * sizeof(UTXO));
			crypto_hash_sha256(txHash.data(), (uint8_t*)inOutHashes.data(), inOutHashes.size());

			// Verify each input
			UTXOKey key;
			uint64_t totalInputAmount = 0;
			uint64_t totalOutputAmount = 0;
			for (TxInputSigned txInputSigned : tx.txInputs) {

				key.txHash = txInputSigned.txInput.txHash;
				key.outputIndex = txInputSigned.txInput.outputIndex;

				// UTXO not found
				if (utxos.count(key) == 0) return false;

				// Invalid signature
				crypto_hash_sha256(txHash.data(), (uint8_t*)&txInputSigned.txInput, sizeof(TxInput));
				if (crypto_sign_verify_detached(txInputSigned.signature.data(), txHash.data(), 32, utxos[key].recipient.data())) return false;

				// Double spending
				if (find(seenUtxo.begin(), seenUtxo.end(), key) != seenUtxo.end()) return false;
				seenUtxo.push_back(key);

				totalInputAmount += utxos[key].amount;
			}

			// Verify each output
			for (UTXO txOutput : tx.txOutputs) {

				totalOutputAmount += txOutput.amount;
			}

			// Output amount exceeds input amount
			if (totalOutputAmount > totalInputAmount) return false;
		}


		// Add block to chain if block is valid
		blockChain[block.blockHash] = block;

		// Remove used UTXOs
		for (Transaction tx : block.transactions) {
			for (TxInputSigned txInputSigned : tx.txInputs) {
				utxos.erase(txInputSigned.txInput.txHash);
			}
		}
		// Add new UTXOs
		for (Transaction tx : block.transactions) {
			for (UTXO txOutput : tx.txOutputs) {
				UTXOKey key;
				key.txHash = txOutput.txHash;
				key.outputIndex = txOutput.outputIndex;
				utxos[key] = txOutput;
			}
		}

		return true;
	}
}

// Unspent Transaction Output Key
struct UTXOKey { 
	array<uint8_t, 32> txHash;
	uint64_t outputIndex;
};

// Unspent Transaction Output
struct UTXO { 
	uint64_t amount;
	array<uint8_t, 32> recipient;
};

// Transaction Input Decides which UTXO to Spend
struct TxInput {
	array<uint8_t, 32> txHash;
	uint64_t outputIndex;
};

// Signed Transaction Input
struct TxInputSigned {
	TxInput txInput;
	array<uint8_t, 32> signature;
};

// Transaction
struct Transaction {
	vector<TxInputSigned> txInputs;
	vector<UTXO> txOutputs;
};

// Block Header
struct BlockHeader {
	uint64_t version;
	array<uint8_t, 32> previousBlockHash;
	array<uint8_t, 32> merkleRoot; // transaction hashes hashed together
	time_t timestamp;
	uint64_t nonce;
	uint64_t difficulty;
};

// Block
struct Block {
	BlockHeader header;
	vector<Transaction> transactions;
	array<uint8_t, 32> blockHash;
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