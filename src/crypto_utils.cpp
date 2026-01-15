#include "crypto_utils.h"
#include <sodium.h>
#include <chrono>

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

std::string bytesToHex(const BytesBuffer& bytes)
{
    std::string hex;
    hex.reserve(bytes.size() * 2);

    for (const auto& byte : bytes)
    {
        constexpr char hexChars[] = "0123456789ABCDEF";
        hex.push_back(hexChars[byte >> 4]);
        hex.push_back(hexChars[byte & 0x0F]);
    }

    return hex;
};
