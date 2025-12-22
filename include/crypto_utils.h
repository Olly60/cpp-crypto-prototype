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
#include <istream>


// ============================================================================
// TYPE ALIASES
// ============================================================================

using Array256_t = std::array<uint8_t, 32>;

using Array512_t = std::array<uint8_t, 64>;

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

    // ------------------------------------------------------------------
    // --- low-level primitives (canonical, unsigned-only) ---
    // ------------------------------------------------------------------

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

    void writeBytesImpl(std::span<const uint8_t> bytes)
    {
        writeU64(bytes.size());
        data_.insert(data_.end(), bytes.data(), bytes.data() + bytes.size());
    }

public:

    BytesBuffer() = default;

    // Set buffer size
    explicit BytesBuffer(size_t size)
        : data_(size)
    {}
    // ------------------------------------------------------------
    // Raw access (inspection only)
    // ------------------------------------------------------------

    [[nodiscard]] const uint8_t* data() const { return data_.data(); }
    [[nodiscard]] uint8_t* data() { return data_.data(); }
    [[nodiscard]] uint64_t size() const { return static_cast<uint64_t>(data_.size()); }
    [[nodiscard]] std::streamsize ssize() const {return static_cast<std::streamsize>(data_.size());}
    [[nodiscard]] const char* cdata() const { return reinterpret_cast<const char*>(data_.data()); }
    [[nodiscard]] char* cdata() { return reinterpret_cast<char*>(data_.data()); }

    void clear() { data_.clear(); read_offset_ = 0; }
    void resetRead() { read_offset_ = 0; }
    void prepareRead(size_t newSize) { data_.resize(newSize); }
    void reserve(size_t newCap) { data_.reserve(newCap); }

    [[nodiscard]] std::string toHex() const { return bytesToHex(*this); }
    [[nodiscard]] std::string toString() const { return std::string(reinterpret_cast<const char*>(data()), size()); }

    // ------------------------------------------------------------
    // Fixed-width integers (explicit)
    // ------------------------------------------------------------

    void writeU8(uint8_t v)   { write_le(v); }
    void writeU16(uint16_t v) { write_le(v); }
    void writeU32(uint32_t v) { write_le(v); }
    void writeU64(uint64_t v) { write_le(v); }

    uint8_t  readU8()  { return read_le<uint8_t>(); }
    uint16_t readU16() { return read_le<uint16_t>(); }
    uint32_t readU32() { return read_le<uint32_t>(); }
    uint64_t readU64() { return read_le<uint64_t>(); }

    // ------------------------------------------------------------
    // Iterator support
    // ------------------------------------------------------------

    // Mutable iterators
    uint8_t* begin() noexcept { return data_.data(); }
    uint8_t* end() noexcept { return data_.data() + data_.size(); }

    // Const iterators
    [[nodiscard]] const uint8_t* begin() const noexcept { return data_.data(); }
    [[nodiscard]] const uint8_t* end() const noexcept { return data_.data() + data_.size(); }

    // ------------------------------------------------------------
    // Semantic byte APIs (preferred)
    // ------------------------------------------------------------

    // Variable-length byte vector
    void writeByteVector(const std::vector<uint8_t>& v)
    {
        writeBytesImpl(v);
    }

    std::vector<uint8_t> readByteVector()
    {
        const uint64_t len = readU64();
        if (read_offset_ + len > data_.size())
            throw std::runtime_error("BytesBuffer: out of bounds");

        std::vector<uint8_t> out(len);
        std::memcpy(out.data(), data_.data() + read_offset_, len);
        read_offset_ += len;
        return out;
    }

    // String (length-prefixed UTF-8 bytes)
    void writeString(const std::string& s)
    {
        if (constexpr size_t MAX_STRING_SIZE = 1 << 20; s.size() > MAX_STRING_SIZE)
            throw std::runtime_error("BytesBuffer: string too large");

        writeBytesImpl(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(s.data()),
            s.size()
        ));
    }

    std::string readString()
    {

        const uint64_t len = readU64();
        if (read_offset_ + len > data_.size())
            throw std::runtime_error("BytesBuffer: out of bounds");

        std::string out(len, '\0');
        std::memcpy(out.data(), data_.data() + read_offset_, len);
        read_offset_ += len;
        return out;
    }

    // Fixed-size 32-byte array (hashes, keys, nonces)
    void writeArray256(const Array256_t& a)
    {
        data_.insert(data_.end(), a.begin(), a.end());
    }

    Array256_t readArray256()
    {

        constexpr size_t SIZE = Array256_t().size();

        if (read_offset_ + SIZE > data_.size())
            throw std::runtime_error("BytesBuffer: out of bounds");

        Array256_t out;
        std::memcpy(out.data(), data_.data() + read_offset_, SIZE);
        read_offset_ += SIZE;
        return out;
    }

    // Fixed-size 64-byte array (signatures)
    void writeArray512(const Array512_t& a)
    {
        data_.insert(data_.end(), a.begin(), a.end());
    }

    Array512_t readArray512()
    {
        constexpr size_t SIZE = Array512_t().size();

        if (read_offset_ + SIZE > data_.size())
            throw std::runtime_error("BytesBuffer: out of bounds");

        Array512_t out;
        std::memcpy(out.data(), data_.data() + read_offset_, SIZE);
        read_offset_ += SIZE;
        return out;
    }

    // BytesBuffer
    void writeBytesBuffer(const BytesBuffer& other)
    {
        writeBytesImpl(other);
    }
};

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

// ============================================================================
// BLOCK WORK
// ============================================================================

// Get block work
Array256_t getBlockWork(BlockHeader& header);

// Decrease difficulty (easier -> shift left)
Array256_t decreaseDifficulty(const Array256_t& arr);

// Increase difficulty (harder -> shift right)
Array256_t increaseDifficulty(const Array256_t& arr);
