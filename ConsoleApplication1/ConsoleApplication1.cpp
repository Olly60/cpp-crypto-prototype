#include <sodium.h>
#include <iostream>
#include <string>
#include <vector>
using namespace std;
uint8_t bytesPrivateKey[crypto_box_SECRETKEYBYTES];
uint8_t bytesPublicKey[crypto_box_PUBLICKEYBYTES];
string hexPrivateKey;
string hexPublicKey;
uint8_t signature[crypto_sign_BYTES];
string command;


void bytesFromHex(uint8_t* out, string hex) {
	for (size_t i = 0; i < hex.size(); i = i + 2) {
		char high = toupper(hex[i]);
		char low = toupper(hex[i + 1]);

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

void hexFromBytes(string &out, uint8_t* bytes, size_t size) {
	out.clear();
	out.resize(size*2);
	for (size_t i = 0; i < size; i++) {
		unsigned char low = bytes[i] >> 4;
		unsigned char high = bytes[i] & 0x0F;
		
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
		out[i*2] = high ;
		out[(i * 2)+1] = low;
	}
}

bool verifyBlock(Block block) {
	uint8_t hash[32];
	crypto_hash_sha256(hash, (uint8_t*)&block.header, sizeof(BlockHeader));
	if (!(block.blockHash == hash)) return false;

	crypto_hash_sha256(hash, (uint8_t*)&block.transactions, sizeof(Transaction)); // combine inputs and outputs into one vector then hash
	if (!(block.header.merkleRoot == hash)) return false;








	return true;

}

struct TxInput {
	uint8_t prevTxHash[32];
	uint64_t outputIndex;
	uint8_t signature[32];
	uint8_t senderPublicKey[32];
};

struct TxOutput {
	uint64_t amount;
	uint8_t publicKey[32];
};

struct Transaction {
	vector<TxInput> txInputs;
	vector<TxOutput> outputs;
	uint8_t transactionHash[32];
};

struct BlockHeader {
	uint8_t previousBlockHash[32];
	uint8_t merkleRoot[32]; // transaction hashes hashed together
	time_t timestamp = time(0);
	uint64_t nonce;
	uint64_t difficulty;
};

struct Block {
	BlockHeader header;
	vector<Transaction> transactions;
	uint8_t blockHash[32];
};

struct UTXO { // for keeping track
	uint64_t outputIndex;
	uint8_t prevtxHash[32];
	uint64_t amount;
	uint8_t publicKey[32];
};


vector<UTXO> UTXOs;
vector<Block> blockChain;

	int main()
	{
		cout << time(0);
		cout << "Enter your private key or type new to generate one: ";
		cin >> command;
		if (command == "new") {
			crypto_box_keypair(bytesPublicKey, bytesPrivateKey);
			hexFromBytes(hexPublicKey, bytesPublicKey, crypto_box_PUBLICKEYBYTES);
			hexFromBytes(hexPrivateKey, bytesPrivateKey, crypto_box_SECRETKEYBYTES);
		}
		else {
			bytesFromHex(bytesPrivateKey, command);
			crypto_scalarmult_base(bytesPublicKey, bytesPrivateKey);
			hexFromBytes(hexPublicKey, bytesPublicKey, crypto_box_PUBLICKEYBYTES);
			hexFromBytes(hexPrivateKey, bytesPrivateKey, crypto_box_SECRETKEYBYTES);
		}
		cout << "Public Key: " << hexPublicKey << endl << "Private Key: " << hexPrivateKey << endl;

		Transaction tx;
		tx.txInputs.push_back(TxInput());
		//crypto_sign_detached(signature, NULL, 'e', 8, bytesPrivateKey);
		//int signVerify = crypto_sign_verify_detached(signature, 'e', 8, bytesPublicKey);

		//cout << signature << endl << signVerify;
	}