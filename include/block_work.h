#pragma once
#include <stop_token>

#include "crypto_utils.h"

Array256_t getBlockWork(const Array256_t& difficulty);

Array256_t addBlockWork(const Array256_t& a, const Array256_t& b);

// Decrease difficulty (easier -> shift left)
Array256_t shiftRight(const Array256_t& arr);

// Increase difficulty (harder -> shift right)
Array256_t shiftLeft(const Array256_t& arr);

void mineBlocks(const std::stop_token& st, const Array256_t& pubKey);