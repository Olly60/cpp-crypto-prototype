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

// Serialise data
std::array<uint8_t, 65> serialiseTxInput(const TxInput& txInput) {
	std::array<uint8_t, 65> serialisedTxInput;
	memcpy(serialisedTxInput.data(), txInput.UTXOTxHash.data(), 32);
	memcpy(serialisedTxInput.data() + 32, putUint64Le(txInput.UTXOOutputIndex).data(), 1);
	memcpy(serialisedTxInput.data() + 33, txInput.signature.data(), 32);
	return serialisedTxInput;
}

std::array<uint8_t, 40> serialiseUTXO(const UTXO& Utxo){
	std::array<uint8_t, 40> serialisedUTXO;
	memcpy(serialisedUTXO.data() + 40, putUint64Le(Utxo.amount).data(), 8);
	memcpy(serialisedUTXO.data() + 48, Utxo.recipient.data(), 32);
	return serialisedUTXO;
}

std::vector<uint8_t> serialiseTx(const Transaction &tx){
	std::vector<uint8_t> serialisedTx;
	std::vector<uint8_t> serialisedInputs;
	for (const TxInput& txInput : tx.txInputs) {
		std::array<uint8_t, 65> serialisedInput = serialiseTxInput(txInput);
		serialisedInputs.insert(serialisedInputs.end(), serialisedInput.begin(), serialisedInput.end());
	}
	std::vector<uint8_t> serialisedOutputs;
	for (const UTXO& txOutput : tx.txOutputs) {
		std::array<uint8_t, 40> serialisedUTXO = serialiseUTXO(txOutput);
		serialisedOutputs.insert(serialisedOutputs.end(), serialisedUTXO.begin(), serialisedUTXO.end());
	}
	serialisedTx.insert(serialisedTx.begin(), serialisedInputs.begin(), serialisedInputs.end());
	serialisedTx.insert(serialisedTx.begin(), serialisedOutputs.begin(), serialisedOutputs.end());
	return serialisedTx;
}

std::vector<uint8_t> serialiseBlock(const Block &block){
	std::vector<uint8_t> serialisedTxs;
	for (const Transaction& tx : block.transactions) {
		std::vector<uint8_t> serialisedTx = serialiseTx(tx);
		serialisedTxs.insert(serialisedTxs.end(), serialisedTx.begin(), serialisedTx.end());
	}
}
