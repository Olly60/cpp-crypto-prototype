#pragma once
#include <filesystem>
#include <fstream>
#include "crypto_utils.h"
#include <rocksdb/db.h>

namespace fs = std::filesystem;

// ============================================================================
// FILE PATHS
// ============================================================================

std::unique_ptr<rocksdb::DB> openDb(const fs::path& path);

BytesBuffer readWholeFile(const fs::path& filePath);

std::ofstream openFileTruncWrite(const fs::path& path);

BytesBuffer readBlockFileBytes(const Array256_t& blockHash);

BytesBuffer readBlockFileHeaderBytes(const Array256_t& blockHash);

bool blockExists(const Array256_t& blockHash);

BlockHeader getBlockHeader(const Array256_t& blockHash);

Block getBlock(const Array256_t& blockHash);

