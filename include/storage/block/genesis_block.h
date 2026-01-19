#pragma once
#include "block.h"
#include "crypto_utils.h"
ChainBlock getGenesisBlock();

Array256_t getGenesisHash();

void initGenesisBlock();