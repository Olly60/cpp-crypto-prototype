#pragma once
#include "storage/block/block_utils.h"

bool verifyTx(const Tx& tx);

bool verifyBlock(const Block& block, const BlockHeader& prevHeader, uint64_t prevTimestamp2);

bool verifyBlockHeader(const BlockHeader& header, const BlockHeader& prevHeader, uint64_t prevTimestamp2);