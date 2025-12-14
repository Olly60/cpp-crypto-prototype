#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include "network/network.h"
#include "storage/block/tip_block.h"
#include "network/request.h"
#include "network/network.h"
#include "storage/peers.h"
#include "storage/block/genesis_block.h"

// ============================================
// Serialization Helpers
// ============================================

uint64_t handshakeSize()
{
    return sizeof(decltype(Handshake::nonce)) + sizeof(decltype(Handshake::blockchainTip)) + sizeof(decltype(Handshake::genesisBlockHash)) + sizeof(decltype(Handshake::services)) + sizeof(decltype(Handshake::version);
}

Handshake createHandshake()
{
    return {
        PROTOCOL_VERSION,
        GENESIS_BLOCK_HASH,
        SERVICE_FULL_NODE,
        LOCAL_NONCE,
        getTipHash()
    };
}

std::vector<uint8_t> serialiseHandshake(const Handshake& hs)
{
    std::vector<uint8_t> handshakeBytes;
    appendBytes(handshakeBytes, hs.version);
    appendBytes(handshakeBytes, hs.genesisBlockHash);
    appendBytes(handshakeBytes, hs.services);
    appendBytes(handshakeBytes, hs.nonce);
    appendBytes(handshakeBytes, hs.blockchainTip);
    return handshakeBytes;
}

Handshake parseHandshake(const std::vector<uint8_t>& buffer)
{
    Handshake hs{};
    size_t offset = 0;
    takeBytesInto(hs.version, buffer, offset);
    takeBytesInto(hs.genesisBlockHash, buffer, offset);
    takeBytesInto(hs.services, buffer, offset);
    takeBytesInto(hs.nonce, buffer, offset);
    takeBytesInto(hs.blockchainTip, buffer, offset);
    return hs;
}

bool isValidHandshake(const Handshake& hs)
{
    return
        hs.version == PROTOCOL_VERSION &&
        hs.genesisBlockHash == GENESIS_BLOCK_HASH &&
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
// Handle connection
// ============================================

asio::awaitable<void> handleConnection(asio::ip::tcp::socket& socket)
{
    try
    {
        const PeerAddress peerAddr{
            socket.remote_endpoint().address().to_string(),
            socket.remote_endpoint().port()
        };

        // Read message type
        uint8_t msgType;
        co_await asio::async_read(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Handle handshake
        if (msgType == 1)
        {
            co_await handleHandshake(socket);
            co_return;
        }

        // Check if peer is authenticated
        if (!peers.contains(peerAddr))
        {
            co_return; // Unauthenticated peer
        }

        // Update last seen
        peers[peerAddr].lastSeen = getCurrentTimestamp();

        // Route message
        switch (static_cast<ProtocolMessage>(msgType))
        {
        case ProtocolMessage::Ping:
            co_await handlePing(socket);
            break;
        case ProtocolMessage::GetHeader:
            co_await handleGetHeader(socket);
            break;
        case ProtocolMessage::GetBlock:
            co_await handleGetBlock(socket);
            break;
        case ProtocolMessage::BroadcastBlock:
            co_await handleNewBlock(socket);
            break;
        case ProtocolMessage::BroadcastMempoolTx:
            co_await handleNewTx(socket);
            break;
        case ProtocolMessage::GetMempool:
            co_await handleGetMempool(socket);
            break;
        case ProtocolMessage::GetHeaders:
            co_await handleGetHeaders(socket);
            break;
        default:
            break; // Unknown message
        }
    }
    catch (const std::exception&)
    {
        // Connection error
    }
}

// ============================================
// Accept connections
// ============================================

asio::awaitable<void> acceptConnections(asio::ip::tcp::acceptor& acceptor)
{
    try
    {
        for (;;)
        {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            co_spawn(acceptor.get_executor(),
                     handleConnection(socket),
                     asio::detached);
        }
    }
    catch (const asio::system_error& e)
    {
        if (e.code() != asio::error::operation_aborted)
            throw;
    }
}


// ============================================
// Main
// ============================================

int main()
{
    asio::io_context ioContext;
    asio::ip::tcp::acceptor acceptor(ioContext,
                                     asio::ip::tcp::endpoint(asio::ip::tcp::v4(), 12345));

    co_spawn(ioContext, acceptConnections(acceptor), asio::detached);

    ioContext.run();

    storePeers(peers);
}
