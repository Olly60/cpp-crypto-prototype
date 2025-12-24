#pragma once
#include "storage/block/block_utils.h"

bool verifyNewTx(const Tx& tx);

bool verifyNewTipBlock(const Block& block);