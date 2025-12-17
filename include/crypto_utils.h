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
    size_t read_offset_ = 0;
public:

    // Constructor
    template <typename... Ts>
    explicit BytesBuffer(Ts&&... values)
    {
        ((*this << std::forward<Ts>(values)), ...);
    }

    // Iterator support
    [[nodiscard]] uint8_t* begin() { return data_.data(); }
    [[nodiscard]] uint8_t* end()   { return data_.data() + data_.size(); }

    [[nodiscard]] const uint8_t* begin() const { return data_.data(); }
    [[nodiscard]] const uint8_t* end() const { return data_.data() + data_.size(); }

    // Access raw data
    [[nodiscard]] uint8_t* data() { return data_.data(); }
    [[nodiscard]] const uint8_t* data() const { return data_.data(); }
    [[nodiscard]] size_t size() const { return data_.size(); }

    // Modify size
    void resize(const size_t n) { data_.resize(n); }

    // Clear buffer
    void clear() { data_.clear(); read_offset_ = 0; }

    // Reset read offset
    void resetRead() { read_offset_ = 0; }

    // Reserve capacity
    void reserve(const size_t n) { data_.reserve(n); }

    // Append raw bytes
    void append(const uint8_t* p, const size_t n) { data_.insert(data_.end(), p, p + n); }
    void append(const std::span<const uint8_t> s) { append(s.data(), s.size()); }

    [[nodiscard]] std::string hexString() const
    {
        return bytesToHex(*this);
    }

    [[nodiscard]] std::string toString() const
    {
        return {this->cdata(), this->size()};
    }

    // Convenience functions
    [[nodiscard]] const char* cdata() const { return reinterpret_cast<const char*>(data()); }
    [[nodiscard]] char* cdata() { return reinterpret_cast<char*>(data()); }
    [[nodiscard]] std::streamsize ssize() const { return static_cast<std::streamsize>(size()); }

    // ------------------------------------------------------------------------
    // --- Write operations ---
    // ------------------------------------------------------------------------

    // Write integral in little-endian
    template <typename T>
    requires std::is_integral_v<std::remove_cvref_t<T>>
    BytesBuffer& operator<<(T&& v)
    {
        if constexpr (std::endian::native == std::endian::big)
        {
            auto* p = reinterpret_cast<uint8_t*>(&v);
            std::reverse(p, p + sizeof(T));
        }
        append(reinterpret_cast<const uint8_t*>(&v), sizeof(T));
        return *this;
    }

    // Write another BytesBuffer
    BytesBuffer& operator<<(const BytesBuffer& other)
    {
        append(other.data(), other.size());
        return *this;
    }

    // Write container of bytes (uint8_t, char, or unsigned char)
    template <typename Container>
    requires std::ranges::contiguous_range<Container>
    BytesBuffer& operator<<(Container&& c)
    {
        append(reinterpret_cast<const uint8_t*>(std::data(c)), std::size(c));
        return *this;
    }

    // ------------------------------------------------------------------------
    // --- Read operations ---
    // ------------------------------------------------------------------------

    // Read integral assuming little-endian wire format
    template <typename T>
    requires std::is_integral_v<std::remove_cvref_t<T>>
    BytesBuffer& operator>>(T& v)
    {
        if (read_offset_ + sizeof(T) > data_.size())
            throw std::runtime_error("ByteBuffer: not enough bytes to read");

        std::memcpy(&v, data_.data() + read_offset_, sizeof(T));
        read_offset_ += sizeof(T);

        if constexpr (std::endian::native == std::endian::big)
        {
            auto* p = reinterpret_cast<uint8_t*>(&v);
            std::reverse(p, p + sizeof(T));
        }

        return *this;
    }

    // Read container of bytes
    template <typename Container>
    requires std::ranges::contiguous_range<Container>
    BytesBuffer& operator>>(Container& c)
    {
        if (read_offset_ + std::size(c) > data_.size())
            throw std::runtime_error("ByteBuffer: not enough bytes to read container");

        std::memcpy(std::data(c), data_.data() + read_offset_, std::size(c));
        read_offset_ += std::size(c);
        return *this;

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
