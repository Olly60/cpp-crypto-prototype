#pragma once
#include <filesystem>
#include "crypto_utils.h"

Array256_t getTipHash();

uint64_t getTipHeight();

Array256_t getTipChainWork();

bool verifyNewMempoolTx(const Tx& tx);

bool verifyNewTipBlock(const Block& block);

void addNewTipBlock(const Block& block);

void undoNewTipBlock(const Block& block);