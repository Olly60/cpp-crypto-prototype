#pragma once
#include "crypto_utils.h"
#include "types.h"
#include <stdexcept>
#include <sodium.h>

// Convert hexadecimal string to byte array
array256_t bytesFromHex(const std::string& hex) {
	array256_t out;
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
	return out;
}

// Convert byte array to hexadecimal string
std::string hexFromBytes(const array256_t& bytes, const uint64_t& len) {
	std::string out;
	out.clear();
	out.resize(len * 2);
	for (uint64_t i = 0; i < len; i++) {
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
	return out;
}

// SHA-256 Hashing
array256_t sha256Of(const void* data, const uint64_t& len) {
	array256_t out;
	crypto_hash_sha256(out.data(), reinterpret_cast<const uint8_t*>(data), len);
	return out;
}

// ============================================================================
// v1 SERIALISERS + PARSERS
// ============================================================================
namespace v1 {
    static std::vector<uint8_t> serialiseTxInput(const TxInput& txInput);

    static TxInput formatTxInput(const uint8_t* data);

    static std::vector<uint8_t> serialiseUTXO(const UTXO& utxo);

    static UTXO formatUTXO(const uint8_t* data);

    static std::vector<uint8_t> serialiseTx(const Tx& tx);

    static Tx formatTx(const uint8_t* data);

    static std::vector<uint8_t> serialiseBlock(const Block& block);

    static Block formatBlock(const uint8_t* data);

    // ----------------------------------------
    // TxInput
    // ----------------------------------------
    static std::vector<uint8_t> serialiseTxInput(const TxInput& txInput) {
        std::vector<uint8_t> out;
        out.insert(out.end(), txInput.UTXOTxHash.begin(), txInput.UTXOTxHash.end());
        out.insert(
            out.end() + txInput.UTXOTxHash.size(),
            reinterpret_cast<const uint8_t*>(&txInput.UTXOOutputIndex),
            reinterpret_cast<const uint8_t*>(&txInput.UTXOOutputIndex) + sizeof(txInput.UTXOOutputIndex)
        );
        out.insert(
            out.end() + txInput.UTXOTxHash.size() + sizeof(txInput.UTXOOutputIndex),
            txInput.signature.begin(),
            txInput.signature.end()
        );
        return out;
    }

    static TxInput formatTxInput(const uint8_t* data) {
        TxInput txInput;
        memcpy(txInput.UTXOTxHash.data(), data, sizeof(txInput.UTXOTxHash));
        memcpy(&txInput.UTXOOutputIndex, data + sizeof(txInput.UTXOTxHash), sizeof(txInput.UTXOOutputIndex));
        memcpy(txInput.signature.data(), data + sizeof(txInput.UTXOOutputIndex) + sizeof(txInput.UTXOTxHash), sizeof(txInput.signature));
        return txInput;
    }

    // ----------------------------------------
    // UTXO
    // ----------------------------------------
    static std::vector<uint8_t> serialiseUTXO(const UTXO& utxo) {
        std::vector<uint8_t> out;
        out.insert(out.end(),
            reinterpret_cast<const uint8_t*>(&utxo.amount),
            reinterpret_cast<const uint8_t*>(&utxo.amount) + sizeof(utxo.amount)
        );
        out.insert(out.end() + sizeof(utxo.amount), utxo.recipient.begin(), utxo.recipient.end());
        return out;
    }

    static UTXO formatUTXO(const uint8_t* data) {
        UTXO utxo;
        memcpy(&utxo.amount, data, sizeof(utxo.amount));
        memcpy(utxo.recipient.data(), data + 32, sizeof(utxo.recipient));
        return utxo;
    }

    // ----------------------------------------
    // Tx
    // ----------------------------------------
    static std::vector<uint8_t> serialiseTx(const Tx& tx) {
        std::vector<uint8_t> out;
        std::vector<uint8_t> inputs;
        std::vector<uint8_t> outputs;

        uint32_t inputCount = 0;
        for (const TxInput& in : tx.txInputs) {
            inputCount++;
            auto v = serialiseTxInput(in);
            inputs.insert(inputs.end(), v.begin(), v.end());
        }

        uint32_t outputCount = 0;
        for (const UTXO& utxo : tx.txOutputs) {
            outputCount++;
            auto v = serialiseUTXO(utxo);
            outputs.insert(outputs.end(), v.begin(), v.end());
        }

        out.insert(out.end(),
            reinterpret_cast<uint8_t*>(&inputCount),
            reinterpret_cast<uint8_t*>(&inputCount) + sizeof(inputCount)
        );
        out.insert(out.end(),
            reinterpret_cast<uint8_t*>(&outputCount),
            reinterpret_cast<uint8_t*>(&outputCount) + sizeof(outputCount)
        );

        out.insert(out.end(), inputs.begin(), inputs.end());
        out.insert(out.end(), outputs.begin(), outputs.end());

        return out;
    }

    static Tx formatTx(const uint8_t* data) {
        Tx tx;

        const uint32_t inputCount = *reinterpret_cast<const uint32_t*>(data);
        const uint32_t outputCount = *reinterpret_cast<const uint32_t*>(data + sizeof(inputCount));

        for (uint32_t i = 0; i < inputCount; i++) {
            tx.txInputs.push_back(
                formatTxInput(data + sizeof(inputCount) + i * inputSize)
            );
        }

        for (uint32_t i = 0; i < outputCount; i++) {
            tx.txOutputs.push_back(
                formatUTXO(
                    data + sizeof(inputCount)
                    + sizeof(outputCount)
                    + inputCount * inputSize
                    + i * outputSize
                )
            );
        }

        return tx;
    }

    // ----------------------------------------
    // Block
    // ----------------------------------------
    static std::vector<uint8_t> serialiseBlock(const Block& block) {
        std::vector<uint8_t> out;

        out.insert(out.end(),
            reinterpret_cast<const uint8_t*>(&block.version),
            reinterpret_cast<const uint8_t*>(&block.version) + sizeof(block.version)
        );
        out.insert(out.end(), block.previousBlockHash.begin(), block.previousBlockHash.end());
        out.insert(out.end(), block.merkleRoot.begin(), block.merkleRoot.end());
        out.insert(out.end(),
            reinterpret_cast<const uint8_t*>(&block.timestamp),
            reinterpret_cast<const uint8_t*>(&block.timestamp) + sizeof(block.timestamp)
        );
        out.insert(out.end(), block.difficulty.begin(), block.difficulty.end());
        out.insert(out.end(), block.nonce.begin(), block.nonce.end());

        std::vector<uint8_t> txBytes;
        uint32_t txCount = 0;

        for (const Tx& t : block.transactions) {
            txCount++;
            auto v = serialiseTx(t);
            txBytes.insert(txBytes.end(), v.begin(), v.end());
        }

        out.insert(out.end(),
            reinterpret_cast<uint8_t*>(&txCount),
            reinterpret_cast<uint8_t*>(&txCount) + sizeof(txCount)
        );
        out.insert(out.end(), txBytes.begin(), txBytes.end());

        return out;
    }

    static Block formatBlock(const uint8_t* data) {
        Block block;
        uint64_t offset = 0;

        memcpy(&block.version, data, sizeof(block.version));
        offset += sizeof(block.version);

        memcpy(block.previousBlockHash.data(), data + offset, sizeof(block.previousBlockHash));
        offset += sizeof(block.previousBlockHash);

        memcpy(block.merkleRoot.data(), data + offset, sizeof(block.merkleRoot));
        offset += sizeof(block.merkleRoot);

        memcpy(&block.timestamp, data + offset, sizeof(block.timestamp));
        offset += sizeof(block.timestamp);

        memcpy(block.difficulty.data(), data + offset, sizeof(block.difficulty));
        offset += sizeof(block.difficulty);

        memcpy(block.nonce.data(), data + offset, sizeof(block.nonce));
        offset += sizeof(block.nonce);

        const uint32_t txCount = *reinterpret_cast<const uint32_t*>(data + offset);
        offset += sizeof(txCount);

        for (uint32_t i = 0; i < txCount; i++) {
            block.transactions.push_back(formatTx(data + offset));
            offset += (sizeof(uint32_t) * 2)
                + *reinterpret_cast<const uint32_t*>(data + offset)
                + *reinterpret_cast<const uint32_t*>(data + offset + sizeof(uint32_t));
        }

        return block;
    }

} // namespace v1

std::vector<uint8_t> serialiseTxInput(const TxInput& in, uint64_t version) {
    switch (version) {
    case 1: return v1::serialiseTxInput(in);
    default: throw std::runtime_error("Unsupported TxInput version");
    }
}

std::vector<uint8_t> serialiseUTXO(const UTXO& utxo, uint64_t version) {
    switch (version) {
    case 1: return v1::serialiseUTXO(utxo);
    default: throw std::runtime_error("Unsupported UTXO version");
    }
}

std::vector<uint8_t> serialiseTx(const Tx& tx, uint64_t version) {
    switch (version) {
    case 1: return v1::serialiseTx(tx);
    default: throw std::runtime_error("Unsupported Tx version");
    }
}

std::vector<uint8_t> serialiseBlock(const Block& block) {
    switch (block.version) {
    case 1: return v1::serialiseBlock(block);
    default: throw std::runtime_error("Unsupported Block version");
    }
}

TxInput formatTxInput(const uint8_t* data, uint64_t version) {
    switch (version) {
    case 1: return v1::formatTxInput(data);
    default: throw std::runtime_error("Unsupported TxInput version");
    }
}

UTXO formatUTXO(const uint8_t* data, uint64_t version) {
    switch (version) {
    case 1: return v1::formatUTXO(data);
    default: throw std::runtime_error("Unsupported UTXO version");
    }
}

Tx formatTx(const uint8_t* data, uint64_t version) {
    switch (version) {
    case 1: return v1::formatTx(data);
    default: throw std::runtime_error("Unsupported Tx version");
    }
}

Block formatBlock(const uint8_t* data) {
    uint64_t version = *reinterpret_cast<const uint64_t*>(data);
    switch (version) {
    case 1: return v1::formatBlock(data);
    default: throw std::runtime_error("Unsupported Block version");
    }
}