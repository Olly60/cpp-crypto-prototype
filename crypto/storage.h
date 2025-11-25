#pragma once

Array256_t getBlockchainTip();

std::vector<Array256_t> getAllBlockHashes();

std::vector<uint8_t> readBlockFile(const Array256_t& blockHash);

void addBlock(const Block& block);

void undoBlock();

bool blockExists(const Array256_t& blockHash);
