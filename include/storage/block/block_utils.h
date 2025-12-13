#pragma once
#include "crypto_utils.h"
void addBlock(const Block& block);

void undoBlock();

bool blockExists(const Array256_t& blockHash);

BlockHeader getBlockHeaderByHash(const Array256_t& blockHash);

BlockHeader getBlockHeaderByHeight(const uint64_t& height);

Block getBlockByHash(const Array256_t& blockHash);