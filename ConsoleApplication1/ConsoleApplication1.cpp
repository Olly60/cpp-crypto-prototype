#include <sodium.h>
#include <iostream>
#include <string>
#include <vector>
using namespace std;
unsigned char bytesPrivateKey[crypto_box_SECRETKEYBYTES];
unsigned char bytesPublicKey[crypto_box_PUBLICKEYBYTES];
string hexPrivateKey;
string hexPublicKey;
unsigned char signature[crypto_sign_BYTES];
string command;
vector<TxInput> TransactionInputs;
vector<TxOutput> TransactionOutputs;
vector<UTXO> UTXOs;

void bytesFromHex(unsigned char* out, string hex) {
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

void hexFromBytes(string &out, const unsigned char* bytes, size_t size) {
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

struct UTXO {
	unsigned char txHash[32];
	uint64_t outputIndex;
	uint64_t amount;
	unsigned char publicKey[32];
};

struct TxInput {
	unsigned char prevTxHash[32];
	uint64_t outputIndex;
	unsigned char signature[32];
};

struct TxOutput {
	uint64_t amount;
	unsigned char publicKey[32];
};

struct Transaction {
	Transaction(uint64_t amountP) {
		uint64_t amount = amountP;
		timestamp = time(0);
	}
	unsigned char txid[32];
	vector<TxInput> inputs;
	vector<TxOutput> outputs;
	time_t timestamp;
};

struct BlockHeader {
	unsigned char previousBlockHash[32];
	unsigned char merkleRoot[32];
	long timestamp;
	int nonce;
	int difficulty;
};

struct Block {
	BlockHeader header;
	vector<Transaction> transactions;
	unsigned char hash[32];
};

struct Blockchain {
	vector<Block> chain;
};

	int main()
	{
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

		Transaction tx(5);

		crypto_sign_detached(signature, NULL, 'e', 8, bytesPrivateKey);
		int signVerify = crypto_sign_verify_detached(signature, 'e', 8, bytesPublicKey);

		cout << signature << endl << signVerify;
	}