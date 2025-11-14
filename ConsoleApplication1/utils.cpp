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
static std::vector<uint8_t> serialiseTxInput(const TxInput& txInput) {
	std::vector<uint8_t> serialisedTxInput;
	serialisedTxInput.insert(serialisedTxInput.end(), txInput.UTXOTxHash.begin(), txInput.UTXOTxHash.end());
	serialisedTxInput.insert(serialisedTxInput.end() + txInput.UTXOTxHash.size(), reinterpret_cast<const uint8_t*>(&txInput.UTXOOutputIndex), reinterpret_cast<const uint8_t*>(&txInput.UTXOOutputIndex) + sizeof(txInput.UTXOOutputIndex));
	serialisedTxInput.insert(serialisedTxInput.end() + txInput.UTXOTxHash.size() + sizeof(txInput.UTXOOutputIndex), txInput.signature.begin(), txInput.signature.end());
	return serialisedTxInput;
}

static std::vector<uint8_t> serialiseUTXO(const UTXO& Utxo){
	std::vector<uint8_t> serialisedUTXO;
	serialisedUTXO.insert(serialisedUTXO.end(), reinterpret_cast<const uint8_t*>(&Utxo.amount), reinterpret_cast<const uint8_t*>(&Utxo.amount) + sizeof(Utxo.amount));
	serialisedUTXO.insert(serialisedUTXO.end() + sizeof(Utxo.amount), Utxo.recipient.begin(), Utxo.recipient.end());
	return serialisedUTXO;
}

static std::vector<uint8_t> serialiseTx(const Transaction &tx){
	std::vector<uint8_t> serialisedTx;
	std::vector<uint8_t> serialisedInputs;
	uint32_t inputAmount = 0;
	for (const TxInput& txInput : tx.txInputs) {
		inputAmount++;
		std::vector<uint8_t> serialisedInput = serialiseTxInput(txInput);
		serialisedInputs.insert(serialisedInputs.end(), serialisedInput.begin(), serialisedInput.end());
	}
	std::vector<uint8_t> serialisedOutputs;
	uint32_t outputAmount = 0;
	for (const UTXO& txOutput : tx.txOutputs) {
		outputAmount++;
		std::vector<uint8_t> serialisedUTXO = serialiseUTXO(txOutput);
		serialisedOutputs.insert(serialisedOutputs.end(), serialisedUTXO.begin(), serialisedUTXO.end());
	}
	serialisedTx.insert(serialisedTx.end(), reinterpret_cast<uint8_t*>(&inputAmount), reinterpret_cast<uint8_t*>(&inputAmount) + sizeof(inputAmount));
	serialisedTx.insert(serialisedTx.end(), reinterpret_cast<uint8_t*>(&outputAmount), reinterpret_cast<uint8_t*>(&outputAmount) + sizeof(outputAmount));
	serialisedTx.insert(serialisedTx.end(), serialisedInputs.begin(), serialisedInputs.end());
	serialisedTx.insert(serialisedTx.end(), serialisedOutputs.begin(), serialisedOutputs.end());
	return serialisedTx;
}

static std::vector<uint8_t> serialiseBlockV1(const Block &block){
	std::vector<uint8_t> serialisedBlock;
	serialisedBlock.insert(serialisedBlock.end(), reinterpret_cast<const uint8_t*>(&block.version), reinterpret_cast<const uint8_t*>(&block.version) + sizeof(block.version));
	serialisedBlock.insert(serialisedBlock.end(), block.previousBlockHash.begin(), block.previousBlockHash.end());
	serialisedBlock.insert(serialisedBlock.end(), block.merkleRoot.begin(), block.merkleRoot.end());
	serialisedBlock.insert(serialisedBlock.end(), reinterpret_cast<const uint8_t*>(&block.timestamp), reinterpret_cast<const uint8_t*>(&block.timestamp) + sizeof(block.timestamp));
	serialisedBlock.insert(serialisedBlock.end(), block.difficulty.begin(), block.difficulty.end());
	serialisedBlock.insert(serialisedBlock.end(), block.nonce.begin(), block.nonce.end());
	std::vector<uint8_t> serialisedTxs;
	std::vector<uint8_t> serialisedTx;
	uint32_t txAmount = 0;
	for (const Transaction& tx : block.transactions) {
		txAmount++;
		std::vector<uint8_t> serialisedTx = serialiseTx(tx);
		serialisedTxs.insert(serialisedTxs.end(), serialisedTx.begin(), serialisedTx.end());
	}
	serialisedBlock.insert(serialisedBlock.end(), reinterpret_cast<uint8_t*>(&txAmount), reinterpret_cast<uint8_t*>(&txAmount) + sizeof(txAmount));
	serialisedBlock.insert(serialisedBlock.end(), serialisedTxs.begin(), serialisedTxs.end());
	return serialisedBlock;
}

std::vector<uint8_t> serialiseBlock(Block &block) {
	uint64_t mainVersion = block.version;
	switch (mainVersion) {
	case 1:
		return serialiseBlockV1(block);
		break;
	}
}

// Format data
static TxInput formatTxInput(const uint8_t* serialisedTxInput) {
	TxInput txInput;
	memcpy(txInput.UTXOTxHash.data(), serialisedTxInput, sizeof(txInput.UTXOTxHash));
	memcpy(&txInput.UTXOOutputIndex, serialisedTxInput + sizeof(txInput.UTXOTxHash), sizeof(txInput.UTXOOutputIndex));
	memcpy(txInput.signature.data(), serialisedTxInput + sizeof(txInput.UTXOOutputIndex) + sizeof(txInput.UTXOTxHash), sizeof(txInput.signature));
	return txInput;
}

static UTXO formatUTXO(const uint8_t *serialisedUtxo) {
	UTXO Utxo;
	memcpy(&Utxo.amount, serialisedUtxo, sizeof(Utxo.amount));
	memcpy(Utxo.recipient.data(), serialisedUtxo + 32, sizeof(Utxo.recipient));
	return Utxo;
}

static Transaction formatTransaction(const uint8_t* serialisedTx) {
	Transaction tx;
	std::vector<TxInput> txInputs;
	std::vector<UTXO> txOutputs;
	const uint32_t inputAmount = *reinterpret_cast<const uint32_t*>(serialisedTx);
	const uint32_t outputAmount = *reinterpret_cast<const uint32_t*>(serialisedTx + sizeof(inputAmount));
	for (uint32_t i = 0; i < inputAmount; i++) {
		txInputs.push_back(formatTxInput(serialisedTx + sizeof(inputAmount) + (i * inputSize)));
	}
	for (uint32_t i = 0; i < outputAmount; i++) {
		txOutputs.push_back(formatUTXO(serialisedTx + sizeof(inputAmount) + sizeof(outputAmount) + (inputAmount * inputSize) + (i * outputSize)));
	}
	tx.txInputs = txInputs;
	tx.txOutputs = txOutputs;
	return tx;
}

static Block formatBlockV1(const uint8_t* serialisedBlock) {
	Block block;
	uint64_t offset = 0;
	memcpy(&block.version, serialisedBlock, sizeof(block.version));
	offset += sizeof(block.version);
	memcpy(block.previousBlockHash.data(), serialisedBlock + offset, sizeof(block.previousBlockHash));
	offset += sizeof(block.previousBlockHash);
	memcpy(block.merkleRoot.data(), serialisedBlock + offset, sizeof(block.merkleRoot));
	offset += sizeof(block.merkleRoot);
	memcpy(&block.timestamp, serialisedBlock + offset, sizeof(block.timestamp));
	offset += sizeof(block.timestamp);
	memcpy(block.difficulty.data(), serialisedBlock + offset, sizeof(block.difficulty));
	offset += sizeof(block.difficulty);
	memcpy(block.nonce.data(), serialisedBlock + offset, sizeof(block.nonce));
	offset += sizeof(block.nonce);
	const uint32_t txAmount = *reinterpret_cast<const uint32_t*>(serialisedBlock + offset);
	offset += sizeof(txAmount);
	Transaction tx;
	for (uint32_t txIndex = 0; txIndex < txAmount; txIndex++) {
		block.transactions.push_back(formatTransaction(serialisedBlock + offset));
		offset += (sizeof(uint32_t) * 2) + *reinterpret_cast<const uint32_t*>(serialisedBlock + offset) + *reinterpret_cast<const uint32_t*>(serialisedBlock + offset + sizeof(uint32_t));
		return block;
	}
}

Block formatBlock(const uint8_t * serialisedBlock) {
	uint64_t mainVersion = *reinterpret_cast<const uint64_t*>(serialisedBlock);
	switch (mainVersion) {
	case 1:
		return formatBlockV1(serialisedBlock);
		break;
	}
}