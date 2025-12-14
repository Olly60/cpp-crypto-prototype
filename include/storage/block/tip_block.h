#pragma once
#include <filesystem>
#include "crypto_utils.h"

void setBlockchainTip(const Array256_t& newTip);

Array256_t getTipHash();