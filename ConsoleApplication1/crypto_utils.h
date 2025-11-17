#pragma once

array256_t bytesFromHex(const std::string& hex);

std::string hexFromBytes(const array256_t& bytes, const uint64_t& len);

array256_t sha256Of(const uint8_t* data, const uint64_t& len);

// Format number to native from little endian
template <typename T>
T formatNumber(const uint8_t* in) {
	T value{};

	// First copy raw bytes into the value (safe for alignment)
	std::memcpy(&value, in, sizeof(T));

	if constexpr (isLittleEndian()) {
		return value; // Already correct
	}
	else {
		// Reverse bytes for big-endian CPUs
		T out{};
		for (size_t i = 0; i < sizeof(T); i++) {
			T byte = (value >> (8 * i)) & 0xFF;
			out |= (byte << (8 * (sizeof(T) - 1 - i)));
		}
		return out;
	}
}


// Serialise number to little endian
template <typename T>
std::array<uint8_t, sizeof(T)> serialiseNumber(const T& in) {
	std::array<uint8_t, sizeof(T)> out{};
	for (size_t i = 0; i < sizeof(T); i++) {
		out[i] = static_cast<uint8_t>(in >> (i * 8));
	}
	return out;
}

std::vector<uint8_t> serialiseTxInput(const TxInput& in, uint64_t version);

std::vector<uint8_t> serialiseUTXO(const UTXO& utxo, uint64_t version);

std::vector<uint8_t> serialiseTx(const Tx& tx, uint64_t version);

std::vector<uint8_t> serialiseBlock(const Block& block);

TxInput formatTxInput(const uint8_t* data, uint64_t version);

UTXO formatUTXO(const uint8_t* data, uint64_t version);

Tx formatTx(const uint8_t* data, uint64_t version);

Block formatBlock(const uint8_t* data);
