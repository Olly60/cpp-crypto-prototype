#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include "network/network_main.h"
#include "tip.h"
#include "network/handle.h"
#include <ranges>
#include "network/network_utils.h"
#include "storage/block/block_indexes.h"
#include "parse_serialise.h"
#include "verify.h"
#include "storage/block/block_utils.h"

asio::awaitable<void> handleGetPeers(asio::ip::tcp::socket& socket)
{
    co_await writeU64Tcp(socket, knownPeers.size());
    co_await writeU64Tcp(socket, knownPeers.size());
    for (const auto& peer: knownPeers)
    {
        if (peer.first.address().is_v4())
        {
            uint8_t ipType = 0x04;
            co_await asio::async_write(socket, asio::buffer(&ipType, 1), asio::use_awaitable);
            co_await asio::async_write(socket, asio::buffer(peer.first.address().to_v4().to_bytes()), asio::use_awaitable);
            BytesBuffer portBuf;
            portBuf.writeU16(peer.first.port());
            co_await asio::async_write(socket, asio::buffer(portBuf.data(), portBuf.size()), asio::use_awaitable);
        } else if (peer.first.address().is_v6())
        {
            uint8_t ipType = 0x06;
            co_await asio::async_write(socket, asio::buffer(&ipType, 1), asio::use_awaitable);
            co_await asio::async_write(socket, asio::buffer(peer.first.address().to_v6().to_bytes()), asio::use_awaitable);
            BytesBuffer portBuf;
            portBuf.writeU16(peer.first.port());
            co_await asio::async_write(socket, asio::buffer(portBuf.data(), portBuf.size()), asio::use_awaitable);
        }
    }
}

asio::awaitable<void> handleGetHeader(asio::ip::tcp::socket& socket)
{
    // Read block hash
    Array256_t blockHash;
    co_await asio::async_read(socket, asio::buffer(blockHash), asio::use_awaitable);

    // Check block is in storage
    auto headerBytes = getBlockHeaderBytes(blockHash);
    if (!headerBytes)
    {
        // Write I dont have it
        uint8_t haveHeader = 0;
        co_await asio::async_write(socket, asio::buffer(&haveHeader, 1), asio::use_awaitable);
        co_return;
    }

    // Tell peer I have it
    uint8_t haveHeader = 1;
    co_await asio::async_write(socket, asio::buffer(&haveHeader, 1), asio::use_awaitable);

    // Send header
    co_await asio::async_write(socket, asio::buffer(headerBytes->data(), headerBytes->size()), asio::use_awaitable);
}

asio::awaitable<void> handleGetBlock(asio::ip::tcp::socket& socket)
{
    // Read block hash
    Array256_t blockHash;
    co_await asio::async_read(socket, asio::buffer(blockHash), asio::use_awaitable);

    // Check block is in storage
    auto blockBytes = getBlockBytes(blockHash);
    if (!blockBytes)
    {
        // Write I dont have it
        uint8_t haveBlock = 0;
        co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);
        co_return;
    }

    // Write have it
    uint8_t haveBlock = 1;
    co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);

    // Write size
    BytesBuffer blockSize;
    blockSize.writeU64(blockBytes->size());
    co_await asio::async_write(socket, asio::buffer(blockSize.data(), blockSize.size()), asio::use_awaitable);

    // Write block
    co_await asio::async_write(socket, asio::buffer(blockBytes->data(), blockBytes->size()), asio::use_awaitable);
}

asio::awaitable<void> handleHandshake(asio::ip::tcp::socket& socket)
{
    // Read peer handshake
    BytesBuffer buffer(calculateHandshakeSize());
    co_await asio::async_read(socket, asio::buffer(buffer.data(), buffer.size()), asio::use_awaitable);

    auto theirHandshake = parseHandshake(buffer);
    // Is handshake valid
    if (!isValidHandshake(theirHandshake))
    {
        co_return;
    }

    // Send our handshake
    auto myHandshake = serialiseHandshake(createHandshake());
    co_await asio::async_write(socket, asio::buffer(myHandshake.data(), myHandshake.size()), asio::use_awaitable);

    // Read verack
    uint8_t theirVerack;
    co_await asio::async_read(socket, asio::buffer(&theirVerack, 1), asio::use_awaitable);
    if (theirVerack != 0x01)
    {
        co_return;
    }

    // Write verack
    constexpr uint8_t myVerack = 0x01;
    co_await asio::async_write(socket, asio::buffer(&myVerack, 1), asio::use_awaitable);

    knownPeers.insert({
    socket.remote_endpoint(), {theirHandshake.services, getCurrentTimestamp(), theirHandshake.relay}
});
}

asio::awaitable<void> handlePing(asio::ip::tcp::socket& socket)
{
    constexpr uint8_t pong = 0x01;
    co_await asio::async_write(socket, asio::buffer(&pong, 1), asio::use_awaitable);
}

asio::awaitable<void> handleGetHeaders(asio::ip::tcp::socket& socket)
{
    // Read amount
    auto peerHashesAmount = co_await readU64Tcp(socket);

    // Read header hashes (Ancestor -> Tip)
    std::vector<Array256_t> blockHashes;
    blockHashes.reserve(peerHashesAmount);
    for (uint64_t i = 0; i < peerHashesAmount; i++)
    {
        Array256_t blockHash;
        co_await asio::async_read(socket, asio::buffer(blockHash), asio::use_awaitable);
        blockHashes.push_back(blockHash);
    }

    // Find common ancestor
    Array256_t commonAncestor;
    for (const auto& hash : blockHashes)
    {
        if (!std::filesystem::exists(getBlockFilePath(hash)))
        {
            commonAncestor = hash;
            break;
        }
    }

    // Find header amount from common ancestor to tip
    auto blockIndexesDb = openBlockIndexesDb();
    uint64_t ancestorHeight = tryGetBlockIndex(*blockIndexesDb, commonAncestor)->height;

    // Amount of headers peer is missing
    uint64_t peerMissingAmount = getTipHeight() - ancestorHeight;

    // Write header amount
    co_await writeU64Tcp(socket, peerMissingAmount);

    // Write headers (peer tip -> next from common ancestor)
    for (auto i = getBlockHeader(getTipHash()); getBlockHeaderHash(*i) != commonAncestor; i = getBlockHeader(
             i->prevBlockHash))
    {
        auto headerBytes = serialiseBlockHeader(i);
        co_await asio::async_write(socket, asio::buffer(headerBytes.data(), headerBytes.size()),
                                   asio::use_awaitable);
    }
}


asio::awaitable<void> handleGetMempool(asio::ip::tcp::socket& socket)
{
    // Write size
    co_await writeU64Tcp(socket, mempool.size());

    // Write inv
    for (const auto& key : mempool | std::views::keys)
    {
        co_await asio::async_write(socket, asio::buffer(key), asio::use_awaitable);
    }

    // Read missing size
    const uint64_t peerMissingCount = co_await readU64Tcp(socket);

    // Read missing hashes
    std::vector<Array256_t> peerMissingHashes;
    peerMissingHashes.reserve(peerMissingCount);
    for (uint64_t i = 0; i < peerMissingCount; i++)
    {
        Array256_t peerMissingHash;
        co_await asio::async_read(socket, asio::buffer(peerMissingHash), asio::use_awaitable);
        peerMissingHashes.push_back(peerMissingHash);
    }

    // Send missing transactions
    std::vector<Tx> peerMissingTxs;
    peerMissingTxs.reserve(peerMissingCount);

    for (const auto key : peerMissingHashes)
    {
        const auto peerMissingTxBytes = serialiseTx(mempool[key]);

        // Send transaction size
        co_await writeU64Tcp(socket, peerMissingTxBytes.size());

        // Send transaction
        co_await asio::async_write(socket, asio::buffer(peerMissingTxBytes.data(), peerMissingTxBytes.size()),
                                   asio::use_awaitable);
    }
}

// ============================================
// Handle new data
// ============================================

asio::awaitable<void> handleNewBlock(asio::ip::tcp::socket& socket)
{
    // Read size
    const uint64_t blockSize = co_await readU64Tcp(socket);

    // Limit block size
    if (blockSize > MAX_BLOCK_SIZE) { co_return; }

    // Read block
    BytesBuffer blockBytes(blockSize);
    co_await asio::async_read(socket, asio::buffer(blockBytes.data(), blockBytes.size()), asio::use_awaitable);

    ChainBlock block = parseBlock(blockBytes);

    // New block doesnt match current tip -> take peers chain if better
    if (block.header.prevBlockHash != getTipHash())
    {
        co_await syncIfBetter(socket);
        co_return;
    }

    // Broadcast to other peers and add block if valid
    if (verifyBlock(block))
    {
        addNewTipBlock(block);
        co_await BroadcastNewBlock(block);
    }
}

asio::awaitable<void> handleNewTx(asio::ip::tcp::socket& socket)
{
    // Read size
    const uint64_t txSize = co_await readU64Tcp(socket);

    // Limit transaction size
    if (txSize > MAX_TX_SIZE) co_return;

    // Read transaction
    BytesBuffer txBytes(txSize);
    co_await asio::async_read(socket, asio::buffer(txBytes.data(), txBytes.size()), asio::use_awaitable);

    // Verify
    Tx tx = parseTx(txBytes);
    if (!verifyTx(tx)) co_return;

    // Add to mempool
    mempool.insert({getTxHash(tx), tx});

    // Broadcast to other peers
    co_await BroadcastNewTx(tx);
}
