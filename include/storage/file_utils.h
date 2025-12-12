#pragma once
#include <filesystem>
#include <fstream>
#include "crypto_utils.h"

namespace fs = std::filesystem;

// ============================================================================
// FILE PATHS
// ============================================================================

namespace paths
{
    const fs::path blockchain = "blockchain";
    const fs::path blockchainTip = blockchain / "blockchain_tip" ;
    const fs::path blocks = blockchain / "blocks";
    const fs::path utxo = blockchain / "utxo";
    const fs::path undo = blockchain / "undo";
    const fs::path peers = blockchain / "peers";
}


std::vector<uint8_t> readWholeFile(const fs::path& filePath);

std::ofstream openFileForAppend(const fs::path& path);

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

