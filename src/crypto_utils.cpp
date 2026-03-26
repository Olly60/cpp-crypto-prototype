#include <sodium.h>
#include <chrono>

#include "crypto_utils.h"

Array256_t sha256Of(const BytesBuffer& data)
{
    Array256_t out;
    crypto_hash_sha256(out.data(), data.data(), data.size());
    return out;
}

uint64_t getCurrentTimestamp()
{
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count()
    );
}

std::string bytesToHex(const void* data, size_t size)
{
    std::string hex;
    hex.reserve(size * 2);

    for (size_t i = 0; i < size; i++)
    {
        constexpr char hexChars[] = "0123456789ABCDEF";
        hex.push_back(hexChars[*(reinterpret_cast<const uint8_t*>(data) + i) >> 4]);
        hex.push_back(hexChars[*(reinterpret_cast<const uint8_t*>(data) + i) & 0x0F]);
    }

    return hex;
}

BytesBuffer hexToBytes(const std::string& hex)
{
    BytesBuffer bytes;

    if (hex.size() % 2 != 0)
        throw std::invalid_argument("hex string must have even length");

    auto hexValue = [](char c) -> uint8_t {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::invalid_argument("invalid hex character");
    };

    for (size_t i = 0; i < hex.size(); i += 2)
    {
        uint8_t byte =
            (hexValue(hex[i]) << 4) |
             hexValue(hex[i + 1]);

        bytes.writeU8(byte);
    }

    return bytes;
}
