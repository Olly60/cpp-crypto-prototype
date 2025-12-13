#pragma once
#include "crypto_utils.h"
// Put block hash by height
void putBlockHeightHash(uint64_t height, const Array256_t& hash);

// Get block hash by height
Array256_t getBlockHeightHash(uint64_t height);

// Delete block by height
void deleteBlockHeightHash(uint64_t height);