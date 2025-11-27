#pragma once
#include <vector>
#include <string>
#include <span>
#include <array>
#include <cstring>
#include <stdexcept>
#include <type_traits>

// ============================================================================
// TYPE ALIASES
// ============================================================================

using Array256_t = std::array<uint8_t, 32>;

using Signature64 = std::array<uint8_t, 64>;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

constexpr size_t calculateBlockHeaderSize() {
    return sizeof(decltype(BlockHeader::version))      // version
        + sizeof(decltype(BlockHeader::prevBlockHash))    // prevBlockHash
        + sizeof(decltype(BlockHeader::merkleRoot))    // merkleRoot
        + sizeof(decltype(BlockHeader::timestamp))      // timestamp
        + sizeof(decltype(BlockHeader::difficulty))    // difficulty
        + sizeof(decltype(BlockHeader::nonce))   // nonce
        + sizeof(decltype(BlockHeader::blockHeight)); // height
}

// Convert hex string to 32-byte array
Array256_t hexToBytes(const std::string& hex);

// Convert 32-byte array to hex string
std::string bytesToHex(const Array256_t& bytes);

// Compute SHA-256 hash of data
Array256_t sha256Of(std::span<const uint8_t> data);

// Get current UNIX timestamp in seconds
uint64_t getCurrentTimestamp();

// ============================================================================
// ENDIANNESS DETECTION
// ============================================================================

// Detect endianness at compile time
constexpr bool isLittleEndian() {
    // Use std::endian in C++20 if available
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
    return __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__;
#else
    // Fallback for older compilers
    constexpr uint16_t x = 1;
    return (*reinterpret_cast<const uint8_t*>(&x)) == 1;
#endif
}

// ============================================================================
// SERIALIZATION HELPERS
// ============================================================================

// Helper for static_assert with dependent types
template<typename T>
inline constexpr bool always_false = false;

/**
 * Deserialize bytes into a value of type T
 * @param out Output value
 * @param data Input byte span
 * @param offset Current position in data (will be updated)
 * @throws std::runtime_error if not enough bytes available
 */
template <typename T>
void takeBytesInto(T& out, std::span<const uint8_t> data, size_t& offset) {
    if (offset + sizeof(T) > data.size()) {
        throw std::runtime_error("takeBytesInto: not enough bytes");
    }

    std::array<uint8_t, sizeof(T)> temp{};
    std::memcpy(temp.data(), data.data() + offset, sizeof(T));

    // Convert from little-endian if needed (for integral types)
    if constexpr (std::is_integral_v<T>) {
        if constexpr (!isLittleEndian()) {
            std::reverse(temp.begin(), temp.end());
        }
    }

    std::memcpy(&out, temp.data(), sizeof(T));
    offset += sizeof(T);
}

/**
 * Deserialize bytes into a value of type T (without offset)
 * @param out Output value
 * @param data Input byte span
 */
template <typename T>
void takeBytesInto(T& out, std::span<const uint8_t> data) {
    size_t offset = 0;
    takeBytesInto(out, data, offset);
}

/**
 * Append serialized representation of data to container
 * - Integral types: serialized as little-endian
 * - Container types: raw bytes appended
 * @param out Output container
 * @param data Data to serialize
 */
template <typename ContainerOut, typename T>
void appendBytes(ContainerOut& out, const T& data) {
    if constexpr (std::is_integral_v<T>) {
        // Integral type: little-endian serialization
        std::array<uint8_t, sizeof(T)> temp{};
        std::memcpy(temp.data(), &data, sizeof(T));

        if constexpr (!isLittleEndian()) {
            std::reverse(temp.begin(), temp.end());
        }

        out.insert(out.end(), temp.begin(), temp.end());
    }
    else if constexpr (requires { std::data(data); std::size(data); }) {
        // Container type: write raw bytes
        const auto* ptr = reinterpret_cast<const uint8_t*>(std::data(data));
        out.insert(out.end(), ptr, ptr + std::size(data));
    }
    else {
        static_assert(always_false<T>, "Type not supported in appendBytes");
    }
}

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * Transaction output (potential UTXO)
 */
struct TxOutput {
    uint64_t amount = 0;
    Array256_t recipient{};
};

/**
 * Transaction input (spends a UTXO)
 */
struct TxInput {
    Array256_t UTXOTxHash{};      // Hash of transaction containing the UTXO
    uint64_t UTXOOutputIndex = 0; // Index of output in that transaction
    Signature64 signature{};        // Signature proving ownership
};

/**
 * Transaction
 */
struct Tx {
    uint64_t version = 1;
    std::vector<TxInput> txInputs;
    std::vector<TxOutput> txOutputs;
};

/**
 * Block header
 */
struct BlockHeader {
    uint64_t version = 1;
    Array256_t prevBlockHash{};
    Array256_t merkleRoot{};
    uint64_t timestamp = 0;
    Array256_t difficulty{};
    Array256_t nonce{};
    uint64_t blockHeight;

    BlockHeader() {
        prevBlockHash.fill(0xFF);
        difficulty.fill(0xFF);
    }
};

/**
 * Block (header + transactions)
 */
struct Block {
    BlockHeader header;
    std::vector<Tx> txs;
};

// ============================================================================
// SERIALIZATION FUNCTIONS
// ============================================================================

// Transaction
std::vector<uint8_t> serialiseTxInput(const TxInput& input);
std::vector<uint8_t> serialiseTxOutput(const TxOutput& output);
std::vector<uint8_t> serialiseTx(const Tx& tx);

TxInput formatTxInput(std::span<const uint8_t> bytes, size_t& offset);
TxOutput formatTxOutput(std::span<const uint8_t> bytes, size_t& offset);
Tx formatTx(std::span<const uint8_t> bytes, size_t& offset);
Tx formatTx(std::span<const uint8_t> bytes);

// Block
std::vector<uint8_t> serialiseBlockHeader(const BlockHeader& header);
std::vector<uint8_t> serialiseBlock(const Block& block);

BlockHeader formatBlockHeader(std::span<const uint8_t> bytes);
Block formatBlock(std::span<const uint8_t> bytes);

// ============================================================================
// HASHING FUNCTIONS
// ============================================================================

// Get hash of a block (from its header)
Array256_t getBlockHash(const Block& block);

// Get hash of a block header
Array256_t getBlockHeaderHash(const BlockHeader& header);

// Get hash of a transaction
Array256_t getTxHash(const Tx& tx);

// Compute merkle root from list of transactions
Array256_t getMerkleRoot(const std::vector<Tx>& txs);

// ============================================================================
// SIGNING
// ============================================================================


Tx signTx(const Tx& tx, const Array256_t& privKeySeed);

bool verifyTxSignature(const Tx& tx);
