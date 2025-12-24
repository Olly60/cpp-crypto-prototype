#pragma once
#include <filesystem>
#include <fstream>
#include "crypto_utils.h"
#include <rocksdb/db.h>

std::optional<BytesBuffer> readFile(const std::filesystem::path& filePath);

std::optional<BytesBuffer> readFile(const std::filesystem::path& filePath, size_t amount);

