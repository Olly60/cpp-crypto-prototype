// ============================================
// Serialization Helpers
// ============================================

#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>
#include <asio.hpp>
#include "network/network_utils.h"
#include "crypto_utils.h"
#include "network/network_main.h"
#include "storage/peers.h"
#include "storage/block/tip_block.h"

constexpr uint64_t handshakeSize()
{
    return sizeof(decltype(Handshake::nonce)) + sizeof(decltype(Handshake::blockchainTip)) + sizeof(decltype(
        Handshake::genesisBlockHash)) + sizeof(decltype(Handshake::services)) + sizeof(decltype(Handshake::version));
}

Handshake createHandshake()
{
    return {
        ProtocolVersion,
        GenesisBlockHash,
        FullNode,
        LOCAL_NONCE,
        getTipHash()
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
    return hs;
}

bool isValidHandshake(const Handshake& hs)
{
    return
        hs.version == ProtocolVersion &&
        hs.genesisBlockHash == GenesisBlockHash &&
        hs.nonce != LOCAL_NONCE;
}

// ============================================
// Add peer to peer map in memory
// ============================================

void addPeerToMemory(const asio::ip::tcp::socket& socket, const Handshake& hs)
{
    const PeerAddress addr{
        socket.remote_endpoint().address().to_string(),
        socket.remote_endpoint().port()
    };

    const PeerStatus status{
        hs.services,
        getCurrentTimestamp()
    };

    peers[addr] = status;
}

// ============================================
// Sync blockchain
// ============================================

asio::awaitable<void> syncIfBetter(asio::ip::tcp::socket& socket)
{
    try
    {
        //TODO: make function

        // if work they claim > then verify else ignore
        //
        //    // TODO: Compare block chain work
        //    requestBlock()
        //    co_await asio::async_read(socket, asio::use_awaitable);
        //
        //    // if their work > our work
        //    // TODO: Remove blocks up to matching one
        //
        //    // TODO: Add new block
    }
    catch (const std::exception&)
    {
    }
}

// ============================================
// Read/Write uint64_t helpers
// ============================================

asio::awaitable<void> writeU64Tcp(asio::ip::tcp::socket& socket, uint64_t v)
{
    BytesBuffer buf;
    buf.writeU64(v);
    co_await asio::async_write(socket, asio::buffer(buf.data(), buf.size()), asio::use_awaitable);
}

asio::awaitable<uint64_t> readU64Tcp(asio::ip::tcp::socket& socket)
{
    BytesBuffer buf;
    buf.prepareRead(sizeof(uint64_t));
    co_await asio::async_read(socket, asio::buffer(buf.data(), buf.size()), asio::use_awaitable);
    co_return buf.readU64();
}

// ============================================
// Broadcast
// ============================================

asio::awaitable<void> BroadcastNewTx(asio::ip::tcp::socket& socket, const Tx& tx)
{
    try
    {
        // Send message type
        auto msgType = static_cast<uint8_t>(ProtocolMessage::BroadcastNewTx);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        auto txBytes = serialiseTx(tx);

        // Send transaction size
        BytesBuffer txSizeBuf;
        txSizeBuf.writeU64(txBytes.size());
        co_await asio::async_write(socket, asio::buffer(txSizeBuf.data(), txSizeBuf.size()), asio::use_awaitable);

        // Send transaction
        co_await asio::async_write(socket, asio::buffer(txBytes.data(), txBytes.size()), asio::use_awaitable);

    }
    catch (const std::exception&)
    {
    }
}

asio::awaitable<void> BroadcastNewBlock(asio::ip::tcp::socket& socket, const Block& block)
{
    try
    {
        // Send message type
        auto msgType = ProtocolMessage::BroadcastNewBlock;
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Send block size
        const auto blockBytes = serialiseBlock(block);
        co_await writeU64Tcp(socket, blockBytes.size());

        // Send block
        co_await asio::async_write(socket, asio::buffer(blockBytes.data(), blockBytes.size()), asio::use_awaitable);

    }
    catch (const std::exception&)
    {
    }
}