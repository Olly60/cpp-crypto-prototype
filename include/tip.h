#pragma once
#include <filesystem>
#include "crypto_utils.h"
const std::filesystem::path TIP = "blockchain_tip";

Array256_t getTipHash();

void addNewTipBlock(const ChainBlock& block);

void undoNewTipBlock();