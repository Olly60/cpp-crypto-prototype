#pragma once
#include <filesystem>
#include "crypto_utils.h"
const std::filesystem::path TIP = "blockchain_tip";
const std::filesystem::path BLOCKS_PATH = "blocks";
const std::filesystem::path UNDO_PATH = "undo";

std::filesystem::path getBlockFilePath(const Array256_t& blockHash);

std::filesystem::path getUndoFilePath(const Array256_t& blockHash);

Array256_t getTipHash();

uint64_t getTipHeight();

Array256_t getTipChainWork();

void addNewTipBlock(const ChainBlock& block);

void undoNewTipBlock();

ChainBlock getTipBlock();

BlockHeader getTipHeader();