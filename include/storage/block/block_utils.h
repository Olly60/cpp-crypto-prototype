#pragma once
#include <filesystem>
#include <optional>
#include "crypto_utils.h"

std::string bytesToHex(const BytesBuffer& bytes);

std::filesystem::path getBlockFilePath(const Array256_t& blockHash);

std::filesystem::path getUndoFilePath(const Array256_t& blockHash);

std::optional<ChainBlock> getBlock(const Array256_t& blockHash);

std::optional<BlockHeader> getBlockHeader(const Array256_t& blockHash);

std::optional<BytesBuffer> getBlockBytes(const Array256_t& blockHash);

std::optional<BytesBuffer> getBlockHeaderBytes(const Array256_t& blockHash);