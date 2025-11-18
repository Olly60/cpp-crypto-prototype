#include <filesystem>
#include <fstream>
#include "types.h"
#include "utils.h"

namespace fs = std::filesystem;

static fs::path findOrMakeFile(const fs::path& baseDir) {
    fs::create_directories(baseDir);

    uint64_t maxIndex = 0;

    for (const auto& entry : fs::directory_iterator(baseDir)) {
        std::vector<uint8_t> readBuf;

        if (!entry.is_regular_file()) continue;

        std::ifstream file(entry.path(), std::ios::binary);
        if (!file) continue;

        // File version
        uint64_t fileVersion;
        readBuf.resize(sizeof(fileVersion));
        file.read(reinterpret_cast<char*>(readBuf.data()), sizeof(fileVersion));
        if (!file) continue;
        fileVersion = formatNumber<uint64_t>(reinterpret_cast<uint8_t*>(readBuf.data()));


        // File Index
        uint64_t fileIndex{};
        readBuf.resize(sizeof(fileIndex));
        file.read(reinterpret_cast<char*>(readBuf.data()), sizeof(fileIndex));
        if (!file) continue;
        fileIndex = formatNumber<uint64_t>(reinterpret_cast<uint8_t*>(fileIndex));

        // Update max index
        if (fileIndex >= maxIndex) maxIndex = fileIndex + 1;

        // Check file size
        std::error_code ec;
        uintmax_t size = fs::file_size(entry.path(), ec);
        if (ec) continue;

        // Apply version-specific rules
        switch (fileVersion) {
        case 1:
            if (size <= 128000000) { // 128 MB limit for version 1
                return entry.path();
            }
            break;
        default:
            continue; // unsupported versions cannot be reused
        }
    }

    // No reusable file found → create new
    std::ostringstream name;
    name << maxIndex << ".dat";
    fs::path newFile = baseDir / name.str();

    std::ofstream out(newFile, std::ios::binary);

    // Write file version (1)
    auto versionBytes = serialiseNumber<uint64_t>(1);
    out.write(reinterpret_cast<char*>(versionBytes.data()), versionBytes.size());

    // Write file index
    auto indexBytes = serialiseNumber<uint64_t>(maxIndex);
    out.write(reinterpret_cast<char*>(indexBytes.data()), indexBytes.size());

    out.close();

    return newFile;
}

namespace v1 {

    static void addBlock(const std::vector<uint8_t> &blockBytes) {
        std::vector<uint8_t> readBuf;
        std::fstream file(findOrMakeFile("chain/blocks", 128000000), std::fstream::binary | std::fstream::app | std::fstream::in);

        // Block amount
        uint64_t blockAmount{};
        readBuf.resize(sizeof(blockAmount));
        file.read(reinterpret_cast<char*>(readBuf.data()), sizeof(blockAmount));
        blockAmount = formatNumber<decltype(blockAmount)>(readBuf.data());
        file.seekg(sizeof(blockAmount), std::ios::cur);

        // Find latest block
        uint32_t blockSize{};
        for (uint64_t i = 0; i < blockAmount; i++) {
            
            file.read(reinterpret_cast<char*>(readBuf.data(), sizeof(blockSize));
            file.seekg(formatNumber<uint32_t>(readBuf.data()), std::ios::cur);
        } 

		
	}
}

void addBlock(const std::vector<uint8_t> &blockBytes) {
    std::fstream file(findOrMakeFile("chain/blocks", 128000000), std::fstream::binary | std::fstream::app | std::fstream::in);
    // File version
    uint64_t fileVersion{};
    std::vector<uint8_t> readBuf;
    readBuf.resize(sizeof(fileVersion));
    file.read(reinterpret_cast<char*>(readBuf.data()), sizeof(fileVersion));
    fileVersion = formatNumber<decltype(fileVersion)>(readBuf.data());
    switch (fileVersion) {
    case 1: v1::addBlock(blockBytes);
    default: throw std::runtime_error("Unsupported file version");
    }

}