#pragma once
#include <filesystem>

#include "block.h"
#include "crypto_utils.h"
const std::filesystem::path TIP = "blockchain_tip.dat";

Array256_t getTipHash();

void addNewTipBlock(const ChainBlock& block);

void undoNewTipBlock();