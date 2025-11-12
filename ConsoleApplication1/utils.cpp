#pragma once
#include "utils.h"
#include <sodium.h>

// Convert hexadecimal string to byte array
void bytesFromHex(const std::string& hex, array256_t& out) {
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
void hexFromBytes(const array256_t& bytes, const uint64_t& size, std::string& out) {
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
void sha256Of(const void* data, const uint64_t& len, array256_t& out) {
	crypto_hash_sha256(out.data(), reinterpret_cast<const uint8_t*>(data), len);
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

void serialiseTx(const Transaction &tx, std::vector<uint8_t> &out){
	std::vector<uint8_t> serialisedTx;
	std::vector<uint8_t> serialisedInputs;
	uint32_t inputAmount = 0;
	for (const TxInput& txInput : tx.txInputs) {
		inputAmount++;
		std::array<uint8_t, 65> serialisedInput = serialiseTxInput(txInput);
		serialisedInputs.insert(serialisedInputs.end(), serialisedInput.begin(), serialisedInput.end());
	}
	std::vector<uint8_t> serialisedOutputs;
	uint32_t outputNum = 0;
	for (const UTXO& txOutput : tx.txOutputs) {
		outputNum++;
		std::array<uint8_t, 40> serialisedUTXO = serialiseUTXO(txOutput);
		serialisedOutputs.insert(serialisedOutputs.end(), serialisedUTXO.begin(), serialisedUTXO.end());
	}
	serialisedTx.push_back(inputAmount)
	serialisedTx.insert(serialisedTx.begin(), serialisedInputs.begin(), serialisedInputs.end());
	serialisedTx.insert(serialisedTx.begin(), serialisedOutputs.begin(), serialisedOutputs.end());
	out = serialisedTx;
}

void serialiseBlock(std::vector<uint8_t>& out, uint32_t &txCount, const Block &block){
	std::vector<uint8_t> serialisedTxs;
	uint32_t txNum;
	uint32_t inputCount;
	uint32_t outputCount;
	std::vector<uint8_t> serialisedTx;
	for (const Transaction& tx : block.transactions) {
		txNum++;
		serialiseTx(serialisedTx, inputCount, outputCount, tx);
		serialisedTxs.insert(serialisedTxs.end(), serialisedTx.begin(), serialisedTx.end());
	}

}

// Format data

TxInput 