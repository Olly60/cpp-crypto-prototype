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
	memcpy(serialisedTxInput.data(), txInput.UTXOTxHash.data(), sizeof(txInput.UTXOTxHash));
	memcpy(serialisedTxInput.data() + txInput.UTXOTxHash.size(), &txInput.UTXOOutputIndex, sizeof(txInput.UTXOOutputIndex));
	memcpy(serialisedTxInput.data() + txInput.UTXOTxHash.size() + sizeof(txInput.UTXOOutputIndex), txInput.signature.data(), sizeof(txInput.signature));
	return serialisedTxInput;
}

std::array<uint8_t, outputSize> serialiseUTXO(const UTXO& Utxo){
	std::array<uint8_t, outputSize> serialisedUTXO;
	memcpy(serialisedUTXO.data(), &Utxo.amount, sizeof(Utxo.amount));
	memcpy(serialisedUTXO.data() + sizeof(Utxo.amount), Utxo.recipient.data(), sizeof(Utxo.recipient));
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
	serialisedTx.insert(serialisedTx.end(), reinterpret_cast<uint8_t*>(&outputAmount), reinterpret_cast<uint8_t*>(&outputAmount) + sizeof(outputAmount));
	serialisedTx.insert(serialisedTx.begin(), serialisedInputs.begin(), serialisedInputs.end());
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
	std::vector<uint8_t> serialisedTxs;
	for (const Transaction& tx : block.transactions) {
		txAmount++;
		serialiseTx(tx, serialisedTxs);
		serialisedTxs.insert(serialisedTxs.end(), serialisedTxs.begin(), serialisedTxs.end());
	}
	serialisedBlock.insert(serialisedBlock.end(), reinterpret_cast<uint8_t*>(&txAmount), reinterpret_cast<uint8_t*>(&txAmount) + sizeof(txAmount)); // Amount of transactions
	serialisedBlock.insert(serialisedBlock.end(), serialisedTxs.begin(), serialisedTxs.end()); // Transactions
}

// Format data

TxInput formatTxInput(const uint8_t* serialisedTxInput) {
	TxInput txInput;
	memcpy(txInput.UTXOTxHash.data(), serialisedTxInput, sizeof(txInput.UTXOTxHash));
	memcpy(&txInput.UTXOOutputIndex, serialisedTxInput + sizeof(txInput.UTXOTxHash), sizeof(txInput.UTXOOutputIndex));
	memcpy(txInput.signature.data(), serialisedTxInput + sizeof(txInput.UTXOOutputIndex) + sizeof(txInput.UTXOTxHash), sizeof(txInput.signature));
}

UTXO formatUTXO(const uint8_t *serialisedUtxo) {
	UTXO Utxo;
	memcpy(&Utxo.amount, serialisedUtxo, sizeof(Utxo.amount));
	memcpy(Utxo.recipient.data(), serialisedUtxo + 32, sizeof(Utxo.recipient));
}

void formatTransaction(const uint8_t* serialisedTx, Transaction &out) {
	Transaction tx;
	std::vector<TxInput> txInputs;e
	std::vector<UTXO> txOutputs;
	uint32_t inputAmount = *reinterpret_cast<const uint32_t*>(serialisedTx);
	uint32_t outputAmount = *reinterpret_cast<const uint32_t*>(serialisedTx + sizeof(inputAmount));
	for (uint32_t i = 0; i < inputAmount; i++) {e
		txInputs.push_back(formatTxInput(serialisedTx + sizeof(inputAmount) + (i * inputSize)));
	}
	for (uint32_t i = 0; i < inputAmount; i++) {
		txOutputs.push_back(formatUTXO(serialisedTx + sizeof(inputAmount) + sizeof(outputAmount) + (inputAmount * inputSize) + (i * outputSize)));
	}
	tx.txInputs = txInputs;
	tx.txOutputs = txOutputs;
	out = tx;
}

void formatBlock(const uint8_t* serialisedBlock ,Block &out) {
	memcpy(&out.version, serialisedBlock, sizeof(out.version));
	memcpy(out.previousBlockHash.data(), serialisedBlock + sizeof(out.version), sizeof(out.previousBlockHash));
	memcpy(out.merkleRoot.data(), serialisedBlock + sizeof(out.version) + sizeof(out.previousBlockHash), sizeof(out.merkleRoot));
	memcpy(&out.timestamp, serialisedBlock + sizeof(out.version) + sizeof(out.previousBlockHash) + sizeof(out.merkleRoot), sizeof(out.timestamp));
	memcpy(out.difficulty.data(), serialisedBlock + sizeof(out.version) + sizeof(out.previousBlockHash) + sizeof(out.merkleRoot) + sizeof(out.timestamp), sizeof(out.difficulty));
	memcpy(out.nonce.data(), serialisedBlock + sizeof(out.version) + sizeof(out.previousBlockHash) + sizeof(out.merkleRoot) + sizeof(out.timestamp) + sizeof(out.difficulty), sizeof(out.nonce));
	const uint8_t *txStart = serialisedBlock + sizeof(out.version) + sizeof(out.previousBlockHash) + sizeof(out.merkleRoot) + sizeof(out.timestamp) + sizeof(out.difficulty) + sizeof(out.nonce);
	uint32_t txAmount = *reinterpret_cast<const uint32_t*>(txStart);
	Transaction tx;
	for (uint32_t txIndex = 0; txIndex < txAmount; txIndex++) {
		formatTransaction(txStart + sizeof(uint32_t), tx);
	}
}