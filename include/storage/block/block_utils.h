#pragma once
#include "crypto_utils.h"
void addBlock(const Block& block);

void undoBlock();

bool blockExists(const Array256_t& blockHash);

BlockHeader getBlockHeaderByHash(const Array256_t& blockHash);

Block getBlockByHash(const Array256_t& blockHash);

BytesBuffer readBlockFileBytes(const Array256_t& blockHash);

BytesBuffer readBlockFileHeaderBytes(const Array256_t& blockHash);