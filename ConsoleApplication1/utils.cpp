#pragma once
#include "utils.h"
#include <sodium.h>

// Convert hexadecimal string to byte array
void bytesFromHex(array256_t& out, const std::string& hex) {
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
void hexFromBytes(std::string& out, const array256_t& bytes, const uint64_t& size) {
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
void sha256Of(array256_t& out, const void* data, const uint64_t& len) {
	crypto_hash_sha256(out.data(), reinterpret_cast<const uint8_t*>(data), len);
}

// Little-endian uint64_t to byte array
std::array<uint8_t, 8> putUint64Le(const uint64_t& value) {
	std::array<uint8_t, 8> buf;
	for (uint8_t i = 0; i < 8; i++) buf[i] = (value >> (i * 8)) & 0xFF;
	return buf;
}

// Sterilise transaction for hashing ----------------- Fix it
void hashTransaction(array256_t& out, const Transaction& tx) {
	std::vector<uint8_t> inOutSerilised;
	for (const TxInput& txInputSigned : tx.txInputs) {
		inOutSerilised.insert(inOutSerilised.end(), txInputSigned.txInput.outputIndex.begin(), txInputSigned.txInput.outputIndex.end());
		inOutSerilised.insert(inOutSerilised.end(), txInputSigned.txInput.prevTxHash.begin(), txInputSigned.txInput.prevTxHash.end());
	}
	for (const UTXO& txOutput : tx.txOutputs) {
		inOutSerilised.insert(inOutSerilised.end(), putUint64Le(txOutput.amount).begin(), putUint64Le(txOutput.amount).end());
		inOutSerilised.insert(inOutSerilised.end(), txOutput.recipient.begin(), txOutput.recipient.end());
	}
	sha256Of(out, inOutSerilised.data(), inOutSerilised.size());
}

 void SerialiseUTXO(std::array<uint8_t, 73> &out , UTXO &Utxo){
	memcpy(,putUint64Le(Utxo.amount))
	
}

 void SerialiseTxInput(std::array<uint8_t, 65>& out, TxInput &txInput){

}

void SerialiseTx(std::array<uint8_t, 35328>& out, Transaction &tx){}

void SerialiseBlock(std::array<uint8_t, 1129680>& out, Block &block){

}
