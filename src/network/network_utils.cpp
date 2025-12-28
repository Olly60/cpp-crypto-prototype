#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio.hpp>
#include "network/network_utils.h"
#include "crypto_utils.h"
#include "network/network_main.h"
#include "storage/peers.h"
#include "tip.h"
#include "network/request.h"
#include "verify.h"
#include "storage/storage_utils.h"
#include "storage/block/block_indexes.h"
#include "storage/block/block_utils.h"

// ============================================
// Handshake Helpers
// ============================================
constexpr uint64_t calculateHandshakeSize()
{
    return sizeof(decltype(Handshake::nonce)) + sizeof(decltype(Handshake::blockchainTip)) + sizeof(decltype(
            Handshake::genesisBlockHash)) + sizeof(decltype(Handshake::services)) + sizeof(decltype(Handshake::version))
        +
        sizeof(decltype(Handshake::relay));
}

Handshake createHandshake()
{
    return {
        LocalProtocolVersion,
        getGenesisBlockHash(),
        Services::FullNode,
        LOCAL_NONCE,
        getTipHash(),
        RELAY
    };
}

BytesBuffer serialiseHandshake(const Handshake& hs)
{
    BytesBuffer handshakeBytes;
    handshakeBytes.writeU64(hs.version);
    handshakeBytes.writeArray256(hs.genesisBlockHash);
    handshakeBytes.writeU64(hs.services);
    handshakeBytes.writeU64(hs.nonce);
    handshakeBytes.writeArray256(hs.blockchainTip);
    handshakeBytes.writeU8(hs.relay);
    return handshakeBytes;
}

Handshake parseHandshake(BytesBuffer& buffer)
{
    Handshake hs{};
    hs.version = buffer.readU64();
    hs.genesisBlockHash = buffer.readArray256();
    hs.services = buffer.readU64();
    hs.nonce = buffer.readU64();
    hs.blockchainTip = buffer.readArray256();
    hs.relay = buffer.readU8();
    return hs;
}

bool isValidHandshake(const Handshake& hs)
{
    return
        hs.version == LocalProtocolVersion &&
        hs.genesisBlockHash == GenesisBlockHash &&
        hs.nonce != LOCAL_NONCE;
}

// ============================================
// Read/Write uint64_t helpers
// ============================================

asio::awaitable<void> writeU64Tcp(asio::ip::tcp::socket& socket, const uint64_t num)
{
    BytesBuffer buf;
    buf.writeU64(num);
    co_await asio::async_write(socket, asio::buffer(buf.data(), buf.size()), asio::use_awaitable);
}

asio::awaitable<uint64_t> readU64Tcp(asio::ip::tcp::socket& socket)
{
    BytesBuffer buf(sizeof(uint64_t));
    co_await asio::async_read(socket, asio::buffer(buf.data(), buf.size()), asio::use_awaitable);
    co_return buf.readU64();
}
