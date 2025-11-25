#pragma once

Array256_t getBlockchainTip();

std::vector<Array256_t> getAllBlockHashes();

void addBlock(const Block& block);

void undoBlock();

bool blockExists(const Array256_t& blockHash);
