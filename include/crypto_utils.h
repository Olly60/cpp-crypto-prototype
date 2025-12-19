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
#include <bit>
#include <istream>

// ============================================================================
// MAIN BUFFER
// ============================================================================

struct BytesBuffer;
// Convert 32-byte array to hex string
std::string bytesToHex(const BytesBuffer& bytes);

struct BytesBuffer
{
private:
    std::vector<uint8_t> data_;
    uint64_t read_offset_ = 0;

public:
    // --- raw access ---
    [[nodiscard]] const uint8_t* data() const { return data_.data(); }
    [[nodiscard]] uint64_t size() const { return static_cast<uint64_t>(data_.size()); }

    void clear() { data_.clear(); read_offset_ = 0; }
    void resetRead() { read_offset_ = 0; }

    // ------------------------------------------------------------------
    // --- low-level primitives (canonical, unsigned-only) ---
    // ------------------------------------------------------------------

    void append(const uint8_t* p, const uint64_t n)
    {
        data_.insert(data_.end(), p, p + n);
    }

    template <typename T>
    void write_le(T v)
    {
        static_assert(std::is_unsigned_v<T>);
        for (uint64_t i = 0; i < sizeof(T); ++i)
            data_.push_back(static_cast<uint8_t>(v >> (8 * i)));
    }

    template <typename T>
    T read_le()
    {
        static_assert(std::is_unsigned_v<T>);
        if (read_offset_ + sizeof(T) > data_.size())
            throw std::runtime_error("BytesBuffer: out of bounds");

        T v = 0;
        for (uint64_t i = 0; i < sizeof(T); ++i)
            v |= static_cast<T>(data_[read_offset_++]) << (8 * i);
        return v;
    }

    // ------------------------------------------------------------------
    // --- explicit API (preferred in consensus code) ---
    // ------------------------------------------------------------------

    void write_u8(const uint8_t v)   { write_le(v); }
    void write_u16(const uint16_t v) { write_le(v); }
    void write_u32(const uint32_t v) { write_le(v); }
    void write_u64(const uint64_t v) { write_le(v); }

    uint8_t  read_u8()  { return read_le<uint8_t>(); }
    uint16_t read_u16() { return read_le<uint16_t>(); }
    uint32_t read_u32() { return read_le<uint32_t>(); }
    uint64_t read_u64() { return read_le<uint64_t>(); }

    void write_bytes(std::span<const uint8_t> bytes)
    {
        write_u64(bytes.size());
        append(bytes.data(), bytes.size());
    }

    std::vector<uint8_t> read_bytes()
    {
        const uint64_t len = read_u64();
        if (read_offset_ + len > data_.size())
            throw std::runtime_error("BytesBuffer: out of bounds");

        const auto offset = static_cast<std::vector<uint8_t>::difference_type>(read_offset_);
        const auto count  = static_cast<std::vector<uint8_t>::difference_type>(len);

        std::vector<uint8_t> out(
            data_.begin() + offset,
            data_.begin() + offset + count
        );

        read_offset_ += len;
        return out;
    }
};




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
BytesBuffer hexToBytes(const std::string& hex);


// Compute SHA-256 hash of data
Array256_t sha256Of(const BytesBuffer& data);

// Get current UNIX timestamp in seconds
uint64_t getCurrentTimestamp();

// ============================================================================
// SERIALIZATION / DESERIALIZATION
// ============================================================================

// ----------------------------------------
// TxInput
// ----------------------------------------
BytesBuffer serialiseTxInput(const TxInput& txInput);

TxInput parseTxInput(BytesBuffer& txInputBytes);
// ----------------------------------------
// TxOutput
// ----------------------------------------
BytesBuffer serialiseTxOutput(const TxOutput& txOutput);

// ----------------------------------------
// Tx
// ----------------------------------------
BytesBuffer serialiseTx(const Tx& tx);

Tx parseTx(BytesBuffer& txBytes);

// ----------------------------------------
// BlockHeader
// ----------------------------------------
BytesBuffer serialiseBlockHeader(const BlockHeader& header);

BlockHeader parseBlockHeader(BytesBuffer& headerBytes);

// ----------------------------------------
// Block
// ----------------------------------------
BytesBuffer serialiseBlock(const Block& block);

Block parseBlock(BytesBuffer& blockBytes);

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
