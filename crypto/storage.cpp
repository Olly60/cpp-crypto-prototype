#include <filesystem>
#include <fstream>
#include <leveldb/db.h>
#include "types.h"
#include "utils.h"

namespace fs = std::filesystem;

static array256_t getLatestBlockHash() {
    std::filesystem::create_directories("chain");
    array256_t latestBlockHash{};
    std::fstream tipFile("chain/tip", std::ios::binary);
    tipFile.read(reinterpret_cast<char*>(latestBlockHash.data()), sizeof(array256_t));
    tipFile.close();
    return latestBlockHash;
}

static fs::path getLatestAvailableFile() {

        leveldb::DB* db;
        leveldb::Options options;
        options.create_if_missing = true;

        leveldb::DB::Open(options, "chain/index", &db);
        std::filesystem::create_directories("chain/index");
        array256_t latestBlockFile{};

        for (auto &file: fs::directory_iterator("chain/index"))
            std::fstream indexFile("chain/index", std::ios::binary | std::ios::ate);



        std::streamsize indexFileSize = indexFile.tellg();
        indexFile.seekg(indexFileSize - 32 - 64, std::ios::beg);

        array256_t latestBlockHash = indexFile.read(latestBlockFile, 32);
        indexFile.close();
        
    return ;
}

namespace v1 {

    static void addBlock(const std::vector<uint8_t> &blockBytes) {
        std::vector<uint8_t> readBuf;
        std::fstream file(getLatestAvailableFile("chain/blocks", 128000000), std::fstream::binary | std::fstream::app | std::fstream::in);

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
    std::fstream file(getLatestAvailableFile("chain/blocks"), std::fstream::binary | std::fstream::app | std::fstream::in);
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