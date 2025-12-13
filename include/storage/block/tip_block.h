#pragma once
#include <filesystem>
#include "crypto_utils.h"

void setBlockchainTip(const Array256_t& newTip, bool undo = false);

Array256_t getTipBlockTipHash();

uint64_t getTipBlockHeight();

Array512_t getTipBlockChainWork();