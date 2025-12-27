#pragma once
#include "network_main.h"

constexpr uint64_t handshakeSize();

Handshake createHandshake();

BytesBuffer serialiseHandshake(const Handshake& hs);

Handshake parseHandshake(BytesBuffer& buffer);

bool isValidHandshake(const Handshake& hs);

// ============================================
// Add peer to peer map in memory
// ============================================

void addPeerToKnown(const asio::ip::tcp::socket& socket, const Handshake& hs);
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
// Read and write helpers
// ============================================

asio::awaitable<uint64_t> readU64Tcp(asio::ip::tcp::socket& socket);

asio::awaitable<void> writeU64Tcp(asio::ip::tcp::socket& socket, uint64_t num);

