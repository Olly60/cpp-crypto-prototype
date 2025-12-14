#include "storage/file_utils.h"

// ============================================================================
// FILE I/O UTILITIES
// ============================================================================

std::ofstream openFileForAppend(const fs::path& path)
{
    fs::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::app | std::ios::binary);
    if (!file)
    {
        throw std::runtime_error("Failed to open file for append: " + path.string());
    }

    // Enable exceptions for future I/O
    file.exceptions(std::ios::failbit | std::ios::badbit);

    return file; // Moved
}


std::vector<uint8_t> readWholeFile(const fs::path& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        throw std::runtime_error("Failed to open file: " + filePath.string());

    const auto size = fs::file_size(filePath);
    std::vector<uint8_t> buffer(size);

    if (size > 0)
    {
        file.read(reinterpret_cast<char*>(buffer.data()),
                  static_cast<std::streamsize>(size));
        if (!file)
            throw std::runtime_error("Failed to read file: " + filePath.string());
    }

    return buffer;
}


std::unique_ptr<rocksdb::DB> openDb(const fs::path& path)
{
    fs::create_directories(path);

    rocksdb::Options options;
    options.create_if_missing = true;

    rocksdb::DB* raw = nullptr;
    const rocksdb::Status status = rocksdb::DB::Open(
        options,
        path.string(),
        &raw
    );

    if (!status.ok() || !raw)
    {
        throw std::runtime_error("Failed to open RocksDB: " + status.ToString());
    }

    return std::unique_ptr<rocksdb::DB>(raw);
}
