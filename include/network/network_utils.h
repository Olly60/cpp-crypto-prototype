#pragma once
#include "network_main.h"

constexpr uint64_t handshakeSize();

Handshake createHandshake();

std::vector<uint8_t> serialiseHandshake(const Handshake& hs);

Handshake parseHandshake(const std::vector<uint8_t>& buffer);

bool isValidHandshake(const Handshake& hs);

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

template <typename T>
requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
asio::awaitable<T> readNumber(asio::ip::tcp::socket& socket)
{
    T size;
    std::array<uint8_t, sizeof(T)> numBuf{};
    co_await asio::async_read(socket, asio::buffer(numBuf), asio::use_awaitable);
    parseBytesInto(size, numBuf);
    co_return size;

}

// ============================================
// Writing helpers
// ============================================

template <typename T>
requires (std::is_integral_v<T> && std::is_unsigned_v<T>)
asio::awaitable<void> writeNumber(asio::ip::tcp::socket& socket, const T size)
{
    std::vector<uint8_t> numBuf;
    numBuf.reserve(sizeof(T));
    serialiseAppendBytes(numBuf, size);
    co_await asio::async_write(socket, asio::buffer(numBuf), asio::use_awaitable);
}

