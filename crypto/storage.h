#pragma once

Array256_t getBlockchainTip();

void addBlock(const Block& block);

void undoBlock();

bool blockExists(const Array256_t& blockHash);
