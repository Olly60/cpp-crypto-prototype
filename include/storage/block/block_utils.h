#pragma once
#include <optional>
#include "crypto_utils.h"
#inc

std::optional<Block> getBlock(const Array256_t& blockHash);

std::optional<BlockHeader> getBlockHeader(const Array256_t& blockHash);

std::optional<BytesBuffer> getBlockBytes(const Array256_t& blockHash);

std::optional<BytesBuffer> getBlockHeaderBytes(const Array256_t& blockHash);