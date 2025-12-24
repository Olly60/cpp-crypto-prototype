#pragma once
#include <filesystem>
#include "crypto_utils.h"

std::filesystem::path getBlockFilePath(const Array256_t& blockHash);

std::filesystem::path getUndoFilePath(const Array256_t& blockHash);

Array256_t getTipHash();

uint64_t getTipHeight();

Array256_t getTipChainWork();

bool verifyNewMempoolTx(const Tx& tx);

bool verifyNewTipBlock(const Block& block);

void addNewTipBlock(const Block& block);

void undoNewTipBlock(const Block& block);