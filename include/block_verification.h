#pragma once
#include "storage/block/block_utils.h"

bool verifyTx(const Tx& tx);

bool verifyBlock(const Block& block, const BlockHeader& prevHeader);

bool verifyBlockHeader(const BlockHeader& header, const BlockHeader& prevHeader);