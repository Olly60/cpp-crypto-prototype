#pragma once

Array256_t getBlockchainTip();

std::vector<uint8_t> readBlockFile(const Array256_t& blockHash);

std::vector<uint8_t> readBlockFileHeader(const Array256_t& blockHash);

void addBlock(const Block& block);

void undoBlock();

bool blockExists(const Array256_t& blockHash);

Array256_t getGenesisBlockHash();
