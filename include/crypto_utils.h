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
template <typename Container>
std::string bytesToHex(const Container& bytes)
{
    static constexpr char hexChars[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bytes.size() * 2);

    for (const auto& byte : bytes)
    {
        out.push_back(hexChars[byte >> 4]);
        out.push_back(hexChars[byte & 0x0F]);
    }

    return out;
}

// Compute SHA-256 hash of data
Array256_t sha256Of(std::span<const uint8_t> data);

// Get current UNIX timestamp in seconds
uint64_t getCurrentTimestamp();

// ============================================================================
// MAIN BUFFER
// ============================================================================

struct BytesBuffer
{
private:
    std::vector<uint8_t> data_;
    size_t read_offset_ = 0;

public:

    template <typename... Ts>
    requires ((std::is_integral_v<Ts> ||
               requires(const Ts& c) { std::ranges::contiguous_range<Ts>; }) && ...)
    explicit BytesBuffer(const Ts&... values)
    {
        ((*this << values), ...);
    }

    [[nodiscard]] uint8_t* data() { return data_.data(); }
    [[nodiscard]] const uint8_t* data() const { return data_.data(); }
    [[nodiscard]] size_t size() const { return data_.size(); }

    void resize(const size_t n) { data_.resize(n); }
    void clear() { data_.clear(); read_offset_ = 0; }
    void resetRead() { read_offset_ = 0; }
    void reserve(const size_t n) { data_.reserve(n); }

    // Append raw bytes
    void append(const uint8_t* p, const size_t n) { data_.insert(data_.end(), p, p + n); }
    void append(const std::span<const uint8_t> s) { append(s.data(), s.size()); }

    [[nodiscard]] std::string toStringHex() const
    {
        return bytesToHex(data_);
    }


    // Write integral in little-endian
    template <typename T>
    requires std::is_integral_v<T>
    BytesBuffer& operator<<(T v)
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

    // Write container of bytes
    template <typename Container>
    requires std::ranges::contiguous_range<Container> &&
         std::same_as<std::remove_cv_t<std::ranges::range_value_t<Container>>, uint8_t>
    BytesBuffer& operator<<(const Container& c)
    {
        append(reinterpret_cast<const uint8_t*>(std::data(c)), std::size(c));
        return *this;
    }

    // Read integral assuming little-endian wire format
    template <typename T>
    requires std::is_integral_v<T>
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
// SERIALIZATION FUNCTIONS
// ============================================================================


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
