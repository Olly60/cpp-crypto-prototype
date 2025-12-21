#pragma once
#include "crypto_utils.h"
void addBlock(const Block& block);

void undoBlock();

bool blockExists(const Array256_t& blockHash);

BlockHeader getBlockHeader(const Array256_t& blockHash);

Block getBlock(const Array256_t& blockHash);

BytesBuffer readBlockFileBytes(const Array256_t& blockHash);

BytesBuffer readBlockFileHeaderBytes(const Array256_t& blockHash);