#include "storage/file_utils.h"

namespace fs = std::filesystem;

// ============================================================================
// FILE I/O UTILITIES
// ============================================================================

std::ofstream openFileTruncWrite(const fs::path& path)
{
    fs::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open file for append: " + path.string());
    }

    // Enable exceptions for future I/O
    file.exceptions(std::ios::failbit | std::ios::badbit);

    return file;
}


BytesBuffer readWholeFile(const fs::path& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open file: " + filePath.string());

    auto size = fs::file_size(filePath);
    BytesBuffer buffer(size);

    if (size > 0)
    {
        file.read(buffer.cdata(), buffer.ssize());
        if (!file)
            throw std::runtime_error("Failed to read file: " + filePath.string());
    }

    return buffer;
}

BytesBuffer readBlockFileBytes(const Array256_t& blockHash)
{
    return readWholeFile(std::string(reinterpret_cast<const char*>(blockHash.data()), blockHash.size()););
}

BytesBuffer readBlockFileHeaderBytes(const Array256_t& blockHash)
{
    const fs::path path = std::string(reinterpret_cast<const char*>(blockHash.data()), blockHash.size());
    if (!fs::exists(path))
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
    return fs::exists(std::string(reinterpret_cast<const char*>(blockHash.data()), blockHash.size()));
}

BlockHeader getBlockHeader(const Array256_t& blockHash)
{
    return parseBlockHeader(readBlockFileHeaderBytes(blockHash));
}

Block getBlock(const Array256_t& blockHash)
{
    return parseBlock(readBlockFileBytes(blockHash));
}

