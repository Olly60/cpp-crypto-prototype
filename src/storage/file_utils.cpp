#include "storage/file_utils.h"

// ============================================================================
// FILE I/O UTILITIES
// ============================================================================

BytesBuffer readFile(const std::filesystem::path& filePath)
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

BytesBuffer readFile(const std::filesystem::path& filePath, size_t amount)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open file: " + filePath.string());

    auto size = std::filesystem::file_size(filePath);
    if (amount > size) amount = size;
    BytesBuffer buffer(amount);

    if (size > 0)
    {
        file.read(buffer.cdata(), amount);
        if (!file) throw std::runtime_error("Failed to read file: " + filePath.string());
    }

    return buffer;
}


