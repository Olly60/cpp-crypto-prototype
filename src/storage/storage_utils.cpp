#include "storage/storage_utils.h"
#include <fstream>

// ============================================================================
// FILE I/O UTILITIES
// ============================================================================

std::optional<BytesBuffer> readFile(const std::filesystem::path& filePath)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file)
        return std::nullopt;

    const std::streamsize size = file.tellg();
    if (size < 0)
        throw std::runtime_error("Failed to stat file: " + filePath.string());

    BytesBuffer buffer(static_cast<size_t>(size));

    if (size > 0)
    {
        file.seekg(0);
        file.read(buffer.cdata(), size);

        if (file.gcount() != size)
            throw std::runtime_error("Short read: " + filePath.string());
    }

    return buffer;
}

std::optional<BytesBuffer> readFile(const std::filesystem::path& filePath, size_t amount)
{
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file)
        return std::nullopt;

    const std::streamsize size = file.tellg();
    if (size < 0)
        throw std::runtime_error("Failed to stat file: " + filePath.string());

    BytesBuffer buffer(amount);

    if (size > 0)
    {
        file.read(buffer.cdata(), amount);

        if (file.gcount() != amount)
            throw std::runtime_error("Short read: " + filePath.string());
    }

    return buffer;
}

void writeFileTrunc(const std::filesystem::path& path, const void* data, size_t size)
{
    // Ensure parent directories exist
    const auto dir = path.parent_path();
    if (!dir.empty()) {
        std::filesystem::create_directories(dir);
    }

    // Open file in binary truncate mode
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        throw std::runtime_error("Failed to open file: " + path.string());
    }

    // Enable exceptions for I/O errors
    file.exceptions(std::ios::failbit | std::ios::badbit);

    // Write the buffer
    file.write(reinterpret_cast<const char*>(data),
               static_cast<std::streamsize>(size));

    // Optional: flush to OS
    file.flush();
}


