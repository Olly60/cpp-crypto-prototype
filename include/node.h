#pragma once
#include <random>
#include <unordered_map>
#include <asio/io_context.hpp>
#include "parse_serialise.h"

// This node's mempool
using MempoolMap = std::unordered_map<Array256_t, Tx, Array256Hash>;
inline MempoolMap mempool;

// IO context
inline asio::io_context ioCtx;

// Protocol version this node is running
constexpr uint64_t LocalProtocolVersion = 1;

// Does this node accept mempool transactions?
constexpr uint8_t RELAY = 1;

// Local nonce to ensure no self connections happen
const uint64_t LOCAL_NONCE = []{ static std::mt19937_64 g(std::random_device{}()); return g(); }();