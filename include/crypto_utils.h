#pragma once
#include <vector>
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>


using Array256_t = std::array<uint8_t, 32>;

using Array512_t = std::array<uint8_t, 64>;

struct Array256Hash
{
    size_t operator()(const Array256_t& a) const
    {
        size_t result = 0;
        for (size_t i = 0; i < 32; i += 8)
        {
            size_t chunk = 0;
            for (size_t j = 0; j < 8; ++j)
            {
                chunk <<= 8;
                chunk |= a[i + j];
            }
            result ^= chunk;
        }
        return result;
    }
};

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
    [[nodiscard]] const char* cdata() const { return reinterpret_cast<const char*>(data_.data()); }
    [[nodiscard]] char* cdata() { return reinterpret_cast<char*>(data_.data()); }

    void clear() { data_.clear(); read_offset_ = 0; }
    void resetRead() { read_offset_ = 0; }
    void prepareRead(size_t newSize) { data_.resize(newSize); }
    void reserve(size_t newCap) { data_.reserve(newCap); }
    void resize(size_t newSize) { data_.resize(newSize); }

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
        writeU64(v.size());
        data_.insert(data_.end(), v.data(), v.data() + v.size());
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

    void writeString(const std::string& s)
    {
        writeU64(s.size());
        data_.insert(data_.end(), s.data(), s.data() + s.size());
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

    template <uint64_t N>
    void writeFixedArray(std::array<uint8_t, N> array)
    {

        data_.insert(data_.end(), array.begin(), array.end());
    }

    template <uint64_t N>
    std::array<uint8_t, N> readFixedArray()
    {
        if (read_offset_ + N > data_.size())
            throw std::runtime_error("BytesBuffer: out of bounds");

        std::array<uint8_t, N> out;
        std::memcpy(out.data(), data_.data() + read_offset_, N);
        read_offset_ += N;
        return out;
    }

    // BytesBuffer
    void writeBytesBuffer(const BytesBuffer& other)
    {
        data_.insert(data_.end(), other.begin(), other.end());
    }

    void insertBytes(const void* begin, const void* end)
    {
        data_.insert(data_.end(), reinterpret_cast<const uint8_t*>(begin), reinterpret_cast<const uint8_t*>(end));
    }
};

Array256_t sha256Of(const BytesBuffer& data);

uint64_t getCurrentTimestamp();

std::string bytesToHex(const BytesBuffer& bytes);

std::string bytesToHex(const Array256_t& bytes);

BytesBuffer hexToBytes(const std::string& hex);