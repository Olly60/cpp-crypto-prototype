#pragma once
#include <filesystem>
#include "crypto_utils.h"
const std::filesystem::path TIP = "blockchain_tip";

Array256_t getTipHash();

uint64_t getTipHeight();

Array256_t getTipChainWork();

void addNewTipBlock(const ChainBlock& block);

void undoNewTipBlock();

ChainBlock getTipBlock();

BlockHeader getTipHeader();