#include "storage/storage_utils.h"

// ============================================================================
// FILE I/O UTILITIES
// ============================================================================

std::optional<BytesBuffer> readFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return std::nullopt;

    const std::streamsize size = file.tellg();
    if (size < 0)
        throw std::runtime_error("Failed to stat file: " + path.string());

    BytesBuffer buffer(static_cast<size_t>(size));

    if (size > 0)
    {
        file.seekg(0);
        file.read(buffer.cdata(), size);

        if (file.gcount() != size)
            throw std::runtime_error("Short read: " + path.string());
    }

    return buffer;
}

std::optional<BytesBuffer> readFile(const std::filesystem::path& path, size_t amount)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return std::nullopt;

    const std::streamsize size = file.tellg();
    if (size < 0)
        throw std::runtime_error("Failed to stat file: " + path.string());

    BytesBuffer buffer(amount);

    if (size > 0)
    {
        file.read(buffer.cdata(), amount);

        if (file.gcount() != amount)
            throw std::runtime_error("Short read: " + path.string());
    }

    return buffer;
}

std::ofstream openFileTruncWrite(const std::filesystem::path& path)
{
    std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::trunc | std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open file for append: " + path.string());
    }

    // Enable exceptions for future I/O
    file.exceptions(std::ios::failbit | std::ios::badbit);

    return file;
}


