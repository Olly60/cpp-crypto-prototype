#pragma once
#include "crypto_utils.h"

struct Handshake
{
    uint64_t version;
    Array256_t genesisBlockHash;
    uint64_t services;
    uint64_t nonce;
    Array256_t blockchainTip;
    uint8_t relay;
    uint16_t port;
};

constexpr uint64_t calculateHandshakeSize()
{
    return sizeof(decltype(Handshake::nonce)) + sizeof(decltype(Handshake::blockchainTip)) + sizeof(decltype(
            Handshake::genesisBlockHash)) + sizeof(decltype(Handshake::services)) + sizeof(decltype(Handshake::version))
        +
        sizeof(decltype(Handshake::relay)) + sizeof(decltype(Handshake::port));
}

Handshake createHandshake();

BytesBuffer serialiseHandshake(const Handshake& hs);

Handshake parseHandshake(BytesBuffer& buffer);

bool isValidHandshake(const Handshake& hs);

// ============================================
// Read and write helpers
// ============================================

asio::awaitable<uint64_t> readU64Tcp(asio::ip::tcp::socket& socket);

asio::awaitable<void> writeU64Tcp(asio::ip::tcp::socket& socket, uint64_t num);

