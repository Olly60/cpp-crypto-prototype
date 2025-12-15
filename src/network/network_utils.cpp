// ============================================
// Serialization Helpers
// ============================================

#include <asio/awaitable.hpp>
#include <asio/ip/tcp.hpp>

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
