#pragma once
#include "crypto_utils.h"
#include <filesystem>
#include "parse_serialise.h"

namespace fs = std::filesystem;

fs::path getBlockFilePath(const Array256_t& blockHash);

fs::path getUndoFilePath(const Array256_t& blockHash);

void addNewTipBlock(const Block& block);

void undoNewTipBlock();

bool blockExists(const Array256_t& blockHash);

BlockHeader getBlockHeader(const Array256_t& blockHash);

Block getBlock(const Array256_t& blockHash);

BytesBuffer readBlockFileBytes(const Array256_t& blockHash);

BytesBuffer readBlockFileHeaderBytes(const Array256_t& blockHash);