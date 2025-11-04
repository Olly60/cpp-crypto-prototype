#include "utils.h"
#include <sodium.h>

using std::vector;
// Convert hexadecimal string to byte array
void bytesFromHex(hash256_t& out, const string& hex) {
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
void hexFromBytes(string& out, const hash256_t& bytes, const uint64_t& size) {
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
void sha256Of(hash256_t& out, const void* data, const uint64_t& len) {
	crypto_hash_sha256(out.data(), reinterpret_cast<const uint8_t*>(data), len);
}

// Little-endian uint64_t to byte array
array<uint8_t, 8> putUint64LE(const uint64_t& value) {
	array<uint8_t, 8> buf;
	for (uint8_t i = 0; i < 8; i++) buf[i] = (value >> (i * 8)) & 0xFF;
	return buf;
}

// Sterilise transaction for hashing
void hashTransaction(hash256_t& out, const Transaction& tx) {
	vector<uint8_t> inOutSerilised;
	for (const TxInputSigned& txInputSigned : tx.txInputs) {
		inOutSerilised.insert(inOutSerilised.end(), putUint64LE(txInputSigned.txInput.outputIndex).begin(), putUint64LE(txInputSigned.txInput.outputIndex).end());
		inOutSerilised.insert(inOutSerilised.end(), txInputSigned.txInput.prevTxHash.begin(), txInputSigned.txInput.prevTxHash.end());
	}
	for (const UTXO& txOutput : tx.txOutputs) {
		inOutSerilised.insert(inOutSerilised.end(), putUint64LE(txOutput.amount).begin(), putUint64LE(txOutput.amount).end());
		inOutSerilised.insert(inOutSerilised.end(), txOutput.recipient.begin(), txOutput.recipient.end());
	}
	sha256Of(out, inOutSerilised.data(), inOutSerilised.size());
}