#pragma once
#include <filesystem>
#include <fstream>
#include "crypto_utils.h"
#include <rocksdb/db.h>

namespace fs = std::filesystem;

// ============================================================================
// FILE PATHS
// ============================================================================

namespace paths
{
    const fs::path blockchain = "blockchain";
    const fs::path blockchainTip = blockchain / "blockchain_tip" ;
    const fs::path blocks = blockchain / "blocks";
    const fs::path utxosDb = blockchain / "utxos";
    const fs::path undo = blockchain / "undo";
    const fs::path peers = blockchain / "peers";
    const fs::path blockHeightsDb = blockchain / "block_heights";
    const fs::path blockIndexesDb = blockchain / "block_indexes";
}

std::unique_ptr<rocksdb::DB> openDb(const fs::path& path);

BytesBuffer readWholeFile(const fs::path& filePath);

std::ofstream openFileTruncWrite(const fs::path& path);

