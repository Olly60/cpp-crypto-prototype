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
std::array<uint8_t, inputSize> serialiseTxInput(const TxInput& txInput) {
	std::array<uint8_t, inputSize> serialisedTxInput;
	memcpy(serialisedTxInput.data(), txInput.UTXOTxHash.data(), txInput.UTXOTxHash.size());
	memcpy(serialisedTxInput.data() + txInput.UTXOTxHash.size(), reinterpret_cast<const uint8_t*>(&txInput.UTXOOutputIndex), sizeof(txInput.UTXOOutputIndex));
	memcpy(serialisedTxInput.data() + txInput.UTXOTxHash.size() + sizeof(txInput.UTXOOutputIndex), txInput.signature.data(), txInput.signature.size());
	return serialisedTxInput;
}

std::array<uint8_t, outputSize> serialiseUTXO(const UTXO& Utxo){
	std::array<uint8_t, outputSize> serialisedUTXO;
	memcpy(serialisedUTXO.data(), reinterpret_cast<const uint8_t*>(&Utxo.amount), sizeof(Utxo.amount));
	memcpy(serialisedUTXO.data() + sizeof(Utxo.amount), Utxo.recipient.data(), Utxo.recipient.size());
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
	uint32_t outputAmount = 0;
	for (const UTXO& txOutput : tx.txOutputs) {
		outputAmount++;
		std::array<uint8_t, outputSize> serialisedUTXO = serialiseUTXO(txOutput);
		serialisedOutputs.insert(serialisedOutputs.end(), serialisedUTXO.begin(), serialisedUTXO.end());
	}
	serialisedTx.insert(serialisedTx.end(), reinterpret_cast<uint8_t*>(&inputAmount), reinterpret_cast<uint8_t*>(&inputAmount) + sizeof(inputAmount));
	serialisedTx.insert(serialisedTx.begin(), serialisedInputs.begin(), serialisedInputs.end());
	serialisedTx.insert(serialisedTx.end(), reinterpret_cast<uint8_t*>(&outputAmount), reinterpret_cast<uint8_t*>(&outputAmount) + sizeof(outputAmount));
	serialisedTx.insert(serialisedTx.begin(), serialisedOutputs.begin(), serialisedOutputs.end());
	out = serialisedTx;
}

void serialiseBlock(std::vector<uint8_t>& out, const Block &block){
	std::vector<uint8_t> serialisedBlock;
	serialisedBlock.insert(serialisedBlock.end(), reinterpret_cast<const uint8_t*>(&block.version), reinterpret_cast<const uint8_t*>(&block.version) + sizeof(block.version));
	serialisedBlock.insert(serialisedBlock.end(), block.previousBlockHash.begin(), block.previousBlockHash.end());
	serialisedBlock.insert(serialisedBlock.end(), block.merkleRoot.begin(), block.merkleRoot.end());
	serialisedBlock.insert(serialisedBlock.end(), reinterpret_cast<const uint8_t*>(&block.timestamp), reinterpret_cast<const uint8_t*>(&block.timestamp) + sizeof(block.timestamp));
	serialisedBlock.insert(serialisedBlock.end(), block.difficulty.begin(), block.difficulty.end());
	serialisedBlock.insert(serialisedBlock.end(), block.nonce.begin(), block.nonce.end());
	std::vector<uint8_t> serialisedTxs;
	uint32_t txAmount = 0;
	std::vector<uint8_t> serialisedTx;
	for (const Transaction& tx : block.transactions) {
		txAmount++;
		serialiseTx(tx, serialisedTx);
		serialisedTxs.insert(serialisedTxs.end(), serialisedTx.begin(), serialisedTx.end());
	}
	serialisedBlock.insert(serialisedBlock.end(), reinterpret_cast<uint8_t*>(&txAmount), reinterpret_cast<uint8_t*>(&txAmount) + sizeof(txAmount));
	serialisedBlock.insert(serialisedBlock.end(), serialisedTx.begin(), serialisedTx.end());
}

// Format data

TxInput formatTxInput(const uint8_t* serialisedTxInput) {
	TxInput txInput;
	memcpy(txInput.UTXOTxHash.data(), serialisedTxInput, sizeof(array256_t));
	memcpy(&txInput.UTXOOutputIndex, serialisedTxInput + sizeof(array256_t), sizeof(uint32_t));
	memcpy(txInput.signature.data(), serialisedTxInput + sizeof(array256_t) + sizeof(uint32_t), sizeof(array256_t));
}

UTXO formatUTXO(const uint8_t *serialisedUtxo) {
	UTXO Utxo;
	memcpy(&Utxo.amount, serialisedUtxo, sizeof(uint32_t));
	memcpy(Utxo.recipient.data(), serialisedUtxo + 32, sizeof(array256_t));
}

void formatTransaction(const uint8_t* serialisedTx, uint32_t size, Transaction &out) {
	Transaction tx;
	std::vector<TxInput> txInputs;
	std::vector<UTXO> txOutputs;
	uint32_t inputAmount;
	uint32_t outputAmount;
	memcpy(&inputAmount, serialisedTx, sizeof(uint32_t));
	memcpy(&outputAmount, serialisedTx + sizeof(uint32_t) + (inputAmount * inputSize), sizeof(uint32_t));
	for (uint32_t i = 0; i < inputAmount; i) {
		txInputs.push_back(formatTxInput(serialisedTx + sizeof(uint32_t) + (i * inputSize)));
	}
	for (uint32_t i = 0; i < inputAmount; i) {
		txOutputs.push_back(formatUTXO(serialisedTx + sizeof(uint32_t) + (inputAmount * inputSize) + sizeof(uint32_t) + (i * outputSize)));
	}
	tx.txInputs = txInputs;
	tx.txOutputs = txOutputs;
	out = tx;
}

void formatBlock(const std::vector<uint8_t> &serialisedBlock, uint32_t size ,Block &out) {
	memcpy
}