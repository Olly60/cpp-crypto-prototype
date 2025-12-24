#pragma once
#include <filesystem>
#include <fstream>
#include "crypto_utils.h"
#include <rocksdb/db.h>

BytesBuffer readFile(const std::filesystem::path& filePath);

BytesBuffer readFile(const std::filesystem::path& filePath, size_t amount);

