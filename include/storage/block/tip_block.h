#pragma once
#include <filesystem>
#include <fstream>
#include "crypto_utils.h"

void setBlockchainTip(const Array256_t& newTip, bool undo = false);

std::pair<Array256_t, uint64_t> getBlockchainTip();