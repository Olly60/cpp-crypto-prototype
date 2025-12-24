#pragma once
#include <filesystem>
#include <fstream>
#include "crypto_utils.h"
#include <rocksdb/db.h>

// ============================================================================
// FILE PATHS
// ============================================================================

std::unique_ptr<rocksdb::DB> openDb(const std::filesystem::path& path);

BytesBuffer readWholeFile(const std::filesystem::path& filePath);

BytesBuffer readBlockFileBytes(const Array256_t& blockHash);

BytesBuffer readBlockFileHeaderBytes(const Array256_t& blockHash);

BlockHeader getBlockHeader(const Array256_t& blockHash);

Block getBlock(const Array256_t& blockHash);

