#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include "network/network_main.h"
#include "storage/block/tip_block.h"
#include "network/handle.h"
#include "storage/peers.h"
// TODO: add limits and safety to network
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
        switch (msgType)
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
        case ProtocolMessage::BroadcastNewBlock:
            co_await handleNewBlock(socket);
            break;
        case ProtocolMessage::BroadcastNewTx:
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
