#include <filesystem>
#include <fstream>
#include "crypto_utils.h"
#include "storage/file_utils.h"
namespace fs = std::filesystem;

// ============================================================================
// FILE PATHS
// ============================================================================

namespace paths
{
    const fs::path blockchain = "blockchain";
    const fs::path blockchainTip = blockchain / "blockchain_tip" / "blockchain_tip";
    const fs::path blocks = blockchain / "blocks";
    const fs::path utxo = blockchain / "utxo";
    const fs::path undo = blockchain / "undo";
    const fs::path peers = blockchain / "peers" / "peers_list";
    const fs::path blockHeight = blockchain / "blockchain_height" / "height";
    const fs::path heightDb = blockchain / "block_heights" / "heights";
}

// ============================================================================
// FILE I/O UTILITIES
// ============================================================================

std::ofstream openFileForAppend(const fs::path& path)
{
    try
    {
        fs::create_directories(path.parent_path());
        std::ofstream file(path, std::ios::app | std::ios::binary);
        if (!file)
        {
            throw std::runtime_error("Failed to open file for append");
        }
        file.exceptions(std::ios::failbit | std::ios::badbit);
        return file;
    }
    catch (const std::ios_base::failure& e)
    {
        throw std::runtime_error("Failed to open file '" + path.string() + "': " + e.what());
    }
    catch (const fs::filesystem_error& e)
    {
        throw std::runtime_error("Filesystem error for path '" + path.string() + "': " + e.what());
    }
}

template <typename T>
void appendToFile(std::ofstream& file, const T& obj)
{
    try
    {
        std::vector<uint8_t> buffer;
        appendBytes(buffer, obj);
        file.write(reinterpret_cast<const char*>(buffer.data()),
                   static_cast<std::streamsize>(buffer.size()));
    }
    catch (const std::ios_base::failure& e)
    {
        throw std::runtime_error("Failed to append to file: " + std::string(e.what()));
    }
}

std::vector<uint8_t> readWholeFile(const fs::path& filePath)
{
    if (!fs::exists(filePath))
    {
        throw std::runtime_error("File does not exist: " + filePath.string());
    }

    std::ifstream file;
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);

    try
    {
        file.open(filePath, std::ios::binary | std::ios::ate);

        std::streamsize size = file.tellg();
        if (size < 0)
        {
            throw std::runtime_error("Failed to determine file size: " + filePath.string());
        }

        file.seekg(0, std::ios::beg);

        std::vector<uint8_t> buffer(static_cast<size_t>(size));
        if (size > 0)
        {
            file.read(reinterpret_cast<char*>(buffer.data()), size);
        }

        return buffer;
    }
    catch (const std::ios_base::failure& e)
    {
        throw std::runtime_error("Failed to read file " + filePath.string() + ": " + e.what());
    }
}
