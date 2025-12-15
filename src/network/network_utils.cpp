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

std::vector<uint8_t> serialiseHandshake(const Handshake& hs)
{
    std::vector<uint8_t> handshakeBytes;
    serialiseAppendBytes(handshakeBytes, hs.version);
    serialiseAppendBytes(handshakeBytes, hs.genesisBlockHash);
    serialiseAppendBytes(handshakeBytes, hs.services);
    serialiseAppendBytes(handshakeBytes, hs.nonce);
    serialiseAppendBytes(handshakeBytes, hs.blockchainTip);
    return handshakeBytes;
}

Handshake parseHandshake(const std::vector<uint8_t>& buffer)
{
    Handshake hs{};
    size_t offset = 0;
    parseBytesInto(hs.version, buffer, offset);
    parseBytesInto(hs.genesisBlockHash, buffer, offset);
    parseBytesInto(hs.services, buffer, offset);
    parseBytesInto(hs.nonce, buffer, offset);
    parseBytesInto(hs.blockchainTip, buffer, offset);
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
// Broadcast
// ============================================

asio::awaitable<void> BroadcastNewTx(asio::ip::tcp::socket& socket, const Tx& tx)
{
    try
    {
        // Send message type
        auto msgType = static_cast<uint8_t>(ProtocolMessage::BroadcastNewTx);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Send transaction size
        const auto txBytes = serialiseTx(tx);
        co_await writeNumber<uint64_t>(socket, txBytes.size());

        // Send transaction
        co_await asio::async_write(socket, asio::buffer(txBytes), asio::use_awaitable);

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
        auto msgType = static_cast<uint8_t>(ProtocolMessage::BroadcastNewBlock);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Send block size
        const auto blockBytes = serialiseBlock(block);
        co_await writeNumber<uint64_t>(socket, blockBytes.size());

        // Send block
        co_await asio::async_write(socket, asio::buffer(blockBytes), asio::use_awaitable);

    }
    catch (const std::exception&)
    {
    }
}
