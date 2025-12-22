#pragma once
#include <filesystem>
#include "crypto_utils.h"

void setBlockchainTip(const Array256_t& newTip);

Array256_t getTipHash();

uint64_t getTipHeight();

Array256_t getTipChainWork();

bool verifyNewBlockTip(const Block& block);