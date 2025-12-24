#include "storage/file_utils.h"

// ============================================================================
// FILE I/O UTILITIES
// ============================================================================


BytesBuffer readWholeFile(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open file: " + filePath.string());

    auto size = std::filesystem::file_size(filePath);
    BytesBuffer buffer(size);

    if (size > 0)
    {
        file.read(buffer.cdata(), buffer.size());
        if (!file)
            throw std::runtime_error("Failed to read file: " + filePath.string());
    }

    return buffer;
}

BytesBuffer readBlockFileBytes(const Array256_t& blockHash)
{
    return readWholeFile(std::string(reinterpret_cast<const char*>(blockHash.data()), blockHash.size()));
}


BytesBuffer readBlockFileHeaderBytes(const Array256_t& blockHash)
{
    const std::filesystem::path path = std::string(reinterpret_cast<const char*>(blockHash.data()), blockHash.size());
    if (!std::filesystem::exists(path))
        throw std::runtime_error("File does not exist: " + path.string());

    std::ifstream file(path, std::ios::binary);
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    constexpr uint64_t headerSize = calculateBlockHeaderSize();
    BytesBuffer header(headerSize);

    file.read(header.cdata(), headerSize);
    return header;
}

bool blockExists(const Array256_t& blockHash)
{
    return std::filesystem::exists(std::string(reinterpret_cast<const char*>(blockHash.data()), blockHash.size()));
}

BlockHeader getBlockHeader(const Array256_t& blockHash)
{
    return parseBlockHeader(readBlockFileHeaderBytes(blockHash));
}

