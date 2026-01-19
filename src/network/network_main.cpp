#include "network/network_main.h"
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include <asio.hpp>
#include <iostream>
#include "node.h"
#include "verify.h"
#include "tip.h"
#include "network/handle.h"
#include "network/network_utils.h"
#include "network/request.h"
#include "storage/peers.h"
#include "storage/storage_utils.h"
#include "storage/block/block_indexes.h"
#include "block.h"
#include "block_work.h"

// ============================================
// Handle connection
// ============================================

asio::awaitable<void> handleConnection(asio::ip::tcp::socket socket)
{
    auto peerAddr = normalizeAddress(socket.remote_endpoint().address());

    for (;;) // Loop until peer closes connection
    {
        // Read the fixed-size message command
        std::array<uint8_t, ProtocolMessage::CommandSize> msgCommand{};
        co_await asio::async_read(socket, asio::buffer(msgCommand), asio::use_awaitable);

        // Handle handshake first
        if (msgCommand == ProtocolMessage::Handshake)
        {
            co_await handleHandshake(socket);
            continue;
        }

        // Check if peer is authenticated
        if (!knownPeers.contains(peerAddr))
        {
            std::cout << "Unknown peer: " << peerAddr.to_string() <<
                " requested something other than a handshake\n";
            co_return; // Unauthenticated peer
        }

        // Update last seen
        knownPeers[peerAddr].lastSeen = getCurrentTimestamp();

        // Route message
        if (msgCommand == ProtocolMessage::Ping)
        {
            co_await handlePing(socket);
        }
        else if (msgCommand == ProtocolMessage::GetBlock)
        {
            co_await handleGetBlock(socket);
        }
        else if (msgCommand == ProtocolMessage::BroadcastNewBlock)
        {
            co_await handleNewBlock(socket);
        }
        else if (msgCommand == ProtocolMessage::BroadcastNewTx)
        {
            co_await handleNewTx(socket);
        }
        else if (msgCommand == ProtocolMessage::GetMempool)
        {
            co_await handleGetMempool(socket);
        }
        else if (msgCommand == ProtocolMessage::GetHeaders)
        {
            co_await handleGetHeaders(socket);
        }
        else if (msgCommand == ProtocolMessage::GetPeers)
        {
            co_await handleGetPeers(socket);
        }
        else
        {
            std::cout << "Unknown message from: " << peerAddr << "\n";
            break;
        }
    }
}

asio::awaitable<void> acceptConnections(uint16_t port)
{
    asio::ip::tcp::acceptor acceptor(ioCtx, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), port));
    try
    {
        for (;;)
        {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);
            std::cout << "Connection from: " << normalizeAddress(socket.remote_endpoint().address()) << "\n";

            // Spawn a coroutine to handle the connection
            co_spawn(ioCtx,
                     handleConnection(std::move(socket)),
                     asio::detached);
        }
    }
    catch (...)
    {
        std::cout << "Error accepting connection\n";
    }
}

asio::awaitable<bool> syncIfBetter(asio::ip::tcp::socket& socket)
{
    co_await asio::post(chainEditStrand, asio::use_awaitable);
    try
    {
        // Handshake to find peer's tip hash
        if (!co_await requestHandshake(socket)) co_return false;

        // Tip already the same as peer's
        if (knownPeers[normalizeAddress(socket.remote_endpoint().address())].tip == getTipHash())
        {
            co_await requestMempool(socket);
            co_await requestPeers(socket);
            co_return true;
        }

        // Get missing headers
        auto headers = co_await requestHeaders(socket);

        if (headers.empty()) co_return true;

        auto commonAncestorHeader = getBlockHeader(headers[0].prevBlockHash);

        // common ancestor doesnt exist
        if (!commonAncestorHeader) co_return false;

        // Get chain work from new headers
        auto peerChainwork = tryGetBlockIndex(commonAncestorHeader->prevBlockHash)->chainWork;

        std::vector<Array256_t> blockHashes;
        blockHashes.reserve(headers.size());
        for (const auto& header : headers)
        {
            peerChainwork = addBlockWork(peerChainwork, getBlockWork(header.difficulty));
            blockHashes.push_back(getBlockHeaderHash(header));
        }

        // Chainwork is lower
        if (tryGetBlockIndex(getTipHash())->chainWork >= peerChainwork) co_return false;

        // Verify first header
        VerifyBlockHeaderContext h0Ctx;
        h0Ctx.prevHeader = &(*commonAncestorHeader);
        uint64_t h0CtxPrevPrevTimestamp = tryGetBlockIndex(headers[0].prevBlockHash)->height > 0 ? getBlockHeader(commonAncestorHeader->prevBlockHash)->timestamp : commonAncestorHeader->timestamp;
        h0Ctx.prevPrevTimestamp = &h0CtxPrevPrevTimestamp;
        if (!verifyBlockHeader(headers[0], h0Ctx)) co_return false;

        // Verify 2nd header if size > 1
        VerifyBlockHeaderContext h1Ctx;
        h1Ctx.prevHeader = &headers[0];
        uint64_t h1CtxPrevPrevTimestamp = commonAncestorHeader->timestamp;
        h1Ctx.prevPrevTimestamp = &h1CtxPrevPrevTimestamp;
        if (headers.size() > 1) { if (!verifyBlockHeader(headers[1], h1Ctx)) co_return false; }

        // Verify all other headers if size > 2
        if (headers.size() > 2)
        {
            for (size_t i = 2; i < headers.size(); ++i)
            {
                VerifyBlockHeaderContext headerCtx;
                headerCtx.prevHeader = &headers[i - 1];
                headerCtx.prevPrevTimestamp = &headers[i - 2].timestamp;
                if (!verifyBlockHeader(headers[i], headerCtx)) co_return false;
            }
        }

        std::filesystem::path tmpBlocksPath = "tmp_blocks";
        std::filesystem::create_directories(tmpBlocksPath);
        auto getTmpBlockPath = [&tmpBlocksPath](const Array256_t& hash) -> std::filesystem::path
        {
            BytesBuffer hashBuf;
            hashBuf.writeArray256(hash);
            return tmpBlocksPath / (bytesToHex(hashBuf.data(), hashBuf.size()) + ".dat");
        };

        // -------------------- Blockchain extension --------------------
        auto commonAncestorHash = getBlockHeaderHash(*commonAncestorHeader);
        auto tipHash = getTipHash();
        if (commonAncestorHash == tipHash)
        {
            for (auto& hash : blockHashes)
            {
                auto block = co_await requestBlock(socket, hash);
                if (!block) co_return false; // peer doesn’t have block

                if (!verifyBlock(*block)) co_return false;

                addNewTipBlock(*block);
            }
            co_await requestMempool(socket);
            co_await requestPeers(socket);
            co_return true; // Done syncing these blocks
        }

        // -------------------- Blockchain reorg --------------------
        // Store blocks
        for (auto& hash : blockHashes)
        {
            // Read block
            auto block = co_await requestBlock(socket, hash);
            if (!block) co_return false; // Peer doesnt have block

            // Write block file
            auto blockBytes = serialiseBlock(*block);
            writeFileTrunc(getTmpBlockPath(hash), blockBytes.data(), blockBytes.size());
        }

        // Verify blocks
        auto readTmpBlockFile = [&getTmpBlockPath](const Array256_t& hash) -> ChainBlock
        {
            auto blockBytes = readFile(getTmpBlockPath(hash));
            return parseBlock(*blockBytes);
        };

        // Transactions context
        std::unordered_set<UTXOId, UTXOIdHash> seenUtxosInDb;
        std::unordered_set<UTXOId, UTXOIdHash> includeUtxos;
        VerifyBlockContext blockCtx;
        blockCtx.txCtx.seenUtxos = &seenUtxosInDb;
        blockCtx.txCtx.includeUtxos = &includeUtxos;

        // Verify first block
        blockCtx.headerCtx = h0Ctx;
        if (!verifyBlock(readTmpBlockFile(blockHashes[0]), blockCtx)) co_return false;

        // Verify 2nd block if size > 1
        blockCtx.headerCtx = h1Ctx;
        if (headers.size() > 1) { if (!verifyBlock(readTmpBlockFile(blockHashes[1]), blockCtx)) co_return false; }

        // Verify all other blocks if size > 2
        if (headers.size() > 2)
        {
            for (size_t i = 2; i < headers.size(); ++i)
            {
                // Header
                blockCtx.headerCtx.prevHeader = &headers[i - 1];
                blockCtx.headerCtx.prevPrevTimestamp = &headers[i - 2].timestamp;
                // Transactions
                if (!verifyBlock(readTmpBlockFile(blockHashes[i]), blockCtx)) co_return false;
            }
        }

        // Verification complete now add to local chain
        // Undo chain up to common ancestor
        while (getTipHash() != commonAncestorHash)
        {
            undoNewTipBlock();
        }


        // Add new blocks from peer
        for (auto& hash : blockHashes)
        {
            addNewTipBlock(readTmpBlockFile(hash));
        }

        std::filesystem::remove_all(tmpBlocksPath);

        co_await requestMempool(socket);
        co_await requestPeers(socket);

        co_return true;
    }
    catch (...)
    {
        co_return false;
    }
}

asio::awaitable<bool> trySyncWithPeers()
{
    std::cout << "Syncing with peers...\n";
    for (auto& peer : knownPeers)
    {
        asio::ip::tcp::socket socket(ioCtx);

        try
        {
            co_await socket.async_connect({peer.first, peer.second.port}, asio::use_awaitable);

            if (!co_await syncIfBetter(socket)) continue;

            std::cout << "Synced with: " << peer.first << "\n";

            co_return true; // synced successfully
        }
        catch (...)
        {
        }
    }
    std::cout << "No better chain found\n";
    co_return false;
}

asio::strand<asio::io_context::executor_type> broadcastStrand{ioCtx.get_executor()};

asio::awaitable<void> broadcastNewTx(asio::io_context& io, Tx tx)
{
    co_await asio::post(broadcastStrand, asio::use_awaitable);

    auto txBytes = serialiseTx(tx);

    for (const auto& peer : knownPeers)
    {
        if (peer.second.relay == 0)
            continue;

        try
        {
            asio::ip::tcp::socket socket(io);

            // Connect to peer
            co_await socket.async_connect({peer.first, peer.second.port}, asio::use_awaitable);

            // Write message type
            co_await asio::async_write(socket, asio::buffer(ProtocolMessage::BroadcastNewTx), asio::use_awaitable);

            // Write size
            co_await writeU64Tcp(socket, txBytes.size());

            // Write transaction
            co_await asio::async_write(socket, asio::buffer(txBytes.data(), txBytes.size()), asio::use_awaitable);
        }
        catch (...)
        {
        }
    }
    co_return;
}


asio::awaitable<void> broadcastNewBlock(asio::io_context& io, ChainBlock block)
{
    co_await asio::post(broadcastStrand, asio::use_awaitable);

    auto blockBytes = serialiseBlock(block);

    for (const auto& peer : knownPeers)
    {
        try
        {
            asio::ip::tcp::socket socket(io);

            // Connect to peer
            co_await socket.async_connect({peer.first, peer.second.port}, asio::use_awaitable);

            // Write message type
            co_await asio::async_write(socket, asio::buffer(ProtocolMessage::BroadcastNewBlock), asio::use_awaitable);

            // Write size
            co_await writeU64Tcp(socket, blockBytes.size());

            // Write block
            co_await asio::async_write(socket, asio::buffer(blockBytes.data(), blockBytes.size()), asio::use_awaitable);
        }
        catch (...)
        {
        }
    }
    co_return;
}
