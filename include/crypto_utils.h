#pragma once
#include <algorithm>
#include <vector>
#include <string>
#include <span>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

// ============================================================================
// TYPE ALIASES
// ============================================================================

using Array256_t = std::array<uint8_t, 32>;

using Array512_t = std::array<uint8_t, 64>;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

struct TxOutput
{
    uint64_t amount = 0;
    Array256_t recipient{};
};

struct TxInput
{
    Array256_t UTXOTxHash{}; // Hash of transaction containing the UTXO
    uint64_t UTXOOutputIndex = 0; // Index of output in that transaction
    Array512_t signature{}; // Signature proving ownership
};

struct Tx
{
    uint64_t version = 1;
    std::vector<TxInput> txInputs;
    std::vector<TxOutput> txOutputs;
};

struct BlockHeader
{
    uint64_t version = 1;
    Array256_t prevBlockHash{};
    Array256_t merkleRoot{};
    uint64_t timestamp = 0;
    Array256_t difficulty{};
    Array256_t nonce{};

    BlockHeader()
    {
        prevBlockHash.fill(0xFF);
        difficulty.fill(0xFF);
        difficulty.back() -= 1;
    }
};

struct Block
{
    BlockHeader header;
    std::vector<Tx> txs;
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

constexpr size_t calculateBlockHeaderSize()
{
    return sizeof(decltype(BlockHeader::version)) // version
        + sizeof(decltype(BlockHeader::prevBlockHash)) // prevBlockHash
        + sizeof(decltype(BlockHeader::merkleRoot)) // merkleRoot
        + sizeof(decltype(BlockHeader::timestamp)) // timestamp
        + sizeof(decltype(BlockHeader::difficulty)) // difficulty
        + sizeof(decltype(BlockHeader::nonce)); // nonce
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
constexpr bool isLittleEndian()
{
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
template <typename T>
inline constexpr bool always_false = false;

/**
 * Deserialize bytes into a value of type T
 * @param out Output value
 * @param data Input byte span
 * @param offset Current position in data (will be updated)
 * @throws std::runtime_error if not enough bytes available
 */
template <typename T>
void parseBytesInto(T& out, const std::span<const uint8_t> data, size_t& offset)
{
    if (offset + sizeof(T) > data.size())
    {
        throw std::runtime_error("takeBytesInto: not enough bytes");
    }

    std::array<uint8_t, sizeof(T)> temp{};
    std::memcpy(temp.data(), data.data() + offset, sizeof(T));

    // Convert from little-endian if needed (for integral types)
    if constexpr (std::is_integral_v<T>)
    {
        if constexpr (!isLittleEndian())
        {
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
void parseBytesInto(T& out, std::span<const uint8_t> data)
{
    size_t offset = 0;
    parseBytesInto(out, data, offset);
}

/**
 * Append serialized representation of data to container
 * - Integral types: serialized as little-endian
 * - Container types: raw bytes appended
 * @param out Output container
 * @param data Data to serialize
 */
template <typename ContainerOut, typename T>
void serialiseAppendBytes(ContainerOut& out, const T& data)
{
    if constexpr (std::is_integral_v<T>)
    {
        // Integral type: little-endian serialization
        std::array<uint8_t, sizeof(T)> temp{};
        std::memcpy(temp.data(), &data, sizeof(T));

        if constexpr (!isLittleEndian())
        {
            std::reverse(temp.begin(), temp.end());
        }

        out.insert(out.end(), temp.begin(), temp.end());
    }
    else if constexpr (requires { std::data(data); std::size(data); })
    {
        // Container type: write raw bytes
        const auto* ptr = reinterpret_cast<const uint8_t*>(std::data(data));
        out.insert(out.end(), ptr, ptr + std::size(data));
    }
    else
    {
        static_assert(always_false<T>, "Type not supported in appendBytes");
    }
}

// ============================================================================
// SERIALIZATION FUNCTIONS
// ============================================================================

// Transaction
std::vector<uint8_t> serialiseTxInput(const TxInput& txInput);
std::vector<uint8_t> serialiseTxOutput(const TxOutput& txOutput);
std::vector<uint8_t> serialiseTx(const Tx& tx);

TxInput parseTxInput(std::span<const uint8_t> txInputBytes, size_t& offset);
TxOutput parseTxOutput(std::span<const uint8_t> txOutputBytes, size_t& offset);
Tx parseTx(std::span<const uint8_t> txBytes, size_t& offset);
Tx parseTx(std::span<const uint8_t> txBytes);

// Block
std::vector<uint8_t> serialiseBlockHeader(const BlockHeader& header);
std::vector<uint8_t> serialiseBlock(const Block& block);

BlockHeader parseBlockHeader(std::span<const uint8_t> headerBytes);
Block parseBlock(std::span<const uint8_t> blockBytes);

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

Array256_t computeTxInputHash(const Tx& tx);

Tx signTxInputs(const Tx& tx, const Array256_t& privKeySeed);

bool verifyTxSignature(const Tx& tx);

// Get block work
Array256_t getBlockWork(BlockHeader header);
