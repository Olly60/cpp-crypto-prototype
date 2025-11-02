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



static void bytesFromHex(array<uint8_t, 32> &out, string hex) {
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

static void hexFromBytes(string &out, array<uint8_t, 32> bytes, size_t size) {
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
	array<uint8_t, 32> hashBuffer;
	if (block.header.version = 1) {
		crypto_hash_sha256(hashBuffer.data(), (uint8_t*)&block.header, sizeof(BlockHeader));
		if (block.blockHash != hashBuffer) return false;
		if (blockChain.count(block.blockHash) == 1) return false;

		for (Transaction tx : block.transactions) {
			for (TxInput txInput : tx.txInputs) {
				crypto_hash_sha256(hashBuffer.data(), (uint8_t*)&txInput, sizeof(TxInput));
				if (crypto_sign_verify_detached(txInput.signature.data(), hashBuffer.data(), 32, txInput.senderPublicKey.data())) return false;
			}



		}




		return true;

	}
}

struct TxInput {
	array<uint8_t, 32> prevTxHash;
	array<uint8_t, 32> outputIndex;
	array<uint8_t, 32> senderPublicKey;
	array<uint8_t, 32> signature;
};

struct TxOutput {
	uint64_t amount;
	array<uint8_t, 32> recipient;
};

struct Transaction {
	vector<TxInput> txInputs;
	vector<TxOutput> txOputs;
	array<uint8_t, 32> transactionHash;
};

struct BlockHeader {
	uint64_t version;
	array<uint8_t, 32> previousBlockHash;
	array<uint8_t, 32> merkleRoot; // transaction hashes hashed together
	time_t timestamp;
	uint64_t nonce;
	uint64_t difficulty;
};

struct Block {
	BlockHeader header;
	vector<Transaction> transactions;
	array<uint8_t, 32> blockHash;
};

struct UTXO { // for keeping track
	uint64_t outputIndex;
	array<uint8_t, 32> prevtxHash;
	uint64_t amount;
	array<uint8_t, 32> publicKey;
};

vector<UTXO> UTXOs;
unordered_map<array<uint8_t, 32>, uint64_t> blockChain;

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


	//crypto_sign_detached(signature, NULL, 'e', 8, bytesPrivateKey);
	//int signVerify = crypto_sign_verify_detached(signature, 'e', 8, bytesPublicKey);

	//cout << signature << endl << signVerify;
}