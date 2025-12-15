#pragma once
#include "network_main.h"

constexpr uint64_t handshakeSize();

Handshake createHandshake();

std::vector<uint8_t> serialiseHandshake(const Handshake& hs);

Handshake parseHandshake(const std::vector<uint8_t>& buffer);

// ============================================
// Add peer to peer map in memory
// ============================================

void addPeerToMemory(const asio::ip::tcp::socket& socket, const Handshake& hs);
// ============================================
// Sync blockchain
// ============================================

asio::awaitable<void> syncIfBetter(asio::ip::tcp::socket& socket);

// ============================================
// Broadcast
// ============================================

asio::awaitable<void> BroadcastNewTx(asio::ip::tcp::socket& socket);

asio::awaitable<void> BroadcastNewBlock(asio::ip::tcp::socket& socket);

// ============================================
// Reading helpers
// ============================================

asio::awaitable<uint64_t> readUint64_t(asio::ip::tcp::socket& socket);

// ============================================
// Writing helpers
// ============================================

asio::awaitable<void> writeUint64_t(asio::ip::tcp::socket& socket, const uint64_t size);

