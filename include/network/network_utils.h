#pragma once
#include "network_main.h"

constexpr uint64_t calculateHandshakeSize();

Handshake createHandshake();

BytesBuffer serialiseHandshake(const Handshake& hs);

Handshake parseHandshake(BytesBuffer& buffer);

bool isValidHandshake(const Handshake& hs);

// ============================================
// Read and write helpers
// ============================================

asio::awaitable<uint64_t> readU64Tcp(asio::ip::tcp::socket& socket);

asio::awaitable<void> writeU64Tcp(asio::ip::tcp::socket& socket, uint64_t num);

