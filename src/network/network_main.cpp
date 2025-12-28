#include "network/network_main.h"
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include <asio.hpp>
#include "parse_serialise.h"
#include "verify.h"
#include "tip.h"
#include "network/handle.h"
#include "network/network_utils.h"
#include "network/request.h"
#include "storage/peers.h"
#include "storage/storage_utils.h"
#include "storage/block/block_indexes.h"
#include "storage/block/block_utils.h"

// TODO: add limits and safety to network and peer bans
// ============================================
// Handle connection
// ============================================

asio::awaitable<void> handleConnection(asio::ip::tcp::socket& socket)
{
    auto peer = socket.remote_endpoint();

    for (;;) // Loop until peer closes connection
    {
        // Read fixed-size message command
        std::array<uint8_t, ProtocolMessage::CommandSize> msgCommand{};
        co_await asio::async_read(socket, asio::buffer(msgCommand), asio::use_awaitable);

        // Handle handshake first
        if (msgCommand == ProtocolMessage::Handshake)
        {
            co_await handleHandshake(socket);
        }

        // Check if peer is authenticated
        if (!knownPeers.contains(peer))
        {
            unknownPeers.insert(peer);
            co_return; // Unauthenticated peer
        }

        // Update last seen
        knownPeers[peer].lastSeen = getCurrentTimestamp();

        // Route message
        if (msgCommand == ProtocolMessage::Ping)
            co_await handlePing(socket);
        else if (msgCommand == ProtocolMessage::GetHeader)
            co_await handleGetHeader(socket);
        else if (msgCommand == ProtocolMessage::GetBlock)
            co_await handleGetBlock(socket);
        else if (msgCommand == ProtocolMessage::BroadcastNewBlock)
            co_await handleNewBlock(socket);
        else if (msgCommand == ProtocolMessage::BroadcastNewTx)
            co_await handleNewTx(socket);
        else if (msgCommand == ProtocolMessage::GetMempool)
            co_await handleGetMempool(socket);
        else if (msgCommand == ProtocolMessage::GetHeaders)
            co_await handleGetHeaders(socket);
        else if (msgCommand == ProtocolMessage::GetPeers)
            co_await handleGetPeers(socket);
        else
            break; // Unknown message
    }
}


// ============================================
// Accept connections
// ============================================

asio::awaitable<void> acceptConnections()
{
    asio::ip::tcp::acceptor acceptor(ioCtx, asio::ip::tcp::endpoint(asio::ip::tcp::v6(), 50000));

    try
    {
        for (;;)
        {
            auto socket = co_await acceptor.async_accept(asio::use_awaitable);

            // Spawn a coroutine to handle the connection
            co_spawn(ioCtx,
                     handleConnection(socket),
                     asio::detached);
        }
    }
    catch (...)
    {
        // log or handle acceptor errors
    }
}

// ============================================
// Sync blockchain
// ============================================

asio::awaitable<bool> syncIfBetter(asio::ip::tcp::socket& socket)
{
    auto headers = co_await requestHeaders(socket);

    // Headers empty blockchain uptodate
    if (headers.empty()) co_return true;

    auto commonAncestorHeader = getBlockHeader(headers[0].prevBlockHash);
    if (!commonAncestorHeader) co_return false; // common ancestor doesnt exist

    // Get that new chain block work
    auto blockIndexesDb = openBlockIndexesDb();
    auto peerChainwork = tryGetBlockIndex(*blockIndexesDb, commonAncestorHeader->difficulty)->chainWork;

    std::vector<Array256_t> blockHashes;
    blockHashes.reserve(headers.size());
    for (const auto& header : headers)
    {
        peerChainwork = addBlockWorkLe(peerChainwork, getBlockWork(header.difficulty));
        blockHashes.push_back(getBlockHeaderHash(header));
    }

    // Chainwork is lower
    if (!isLessLE(getTipChainWork(), peerChainwork)) co_return false;

    // Verify first header
    VerifyBlockHeaderContext h0Ctx;
    h0Ctx.prevHeader = &*commonAncestorHeader;
    uint64_t h0CtxTimestamp = getBlockHeader(commonAncestorHeader->prevBlockHash)->timestamp;
    h0Ctx.prevPrevTimestamp = &h0CtxTimestamp;
    if (!verifyBlockHeader(headers[0], h0Ctx)) co_return false;

    // Verify 2nd header if size > 1
    VerifyBlockHeaderContext h1Ctx;
    h1Ctx.prevHeader = &headers[0];
    uint64_t h1CtxTimestamp = getBlockHeader(headers[0].prevBlockHash)->timestamp;
    h1Ctx.prevPrevTimestamp = &h1CtxTimestamp;
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

    std::filesystem::path tmpBlocksPath = "tmp_blockchain";
    auto getTmpBlockPath = [&tmpBlocksPath](const Array256_t& hash) -> std::filesystem::path
    {
        BytesBuffer hashBuf;
        hashBuf.writeArray256(hash);
        return tmpBlocksPath / (bytesToHex(hashBuf) + ".block");
    };

    // ----------------------------------------
    // Blockchain extension
    // ----------------------------------------
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
        co_return true; // Done syncing these blocks
    }

    // ----------------------------------------
    // Blockchain reorg
    // ----------------------------------------
    // Store blocks
    for (auto& hash : blockHashes)
    {
        // Read block
        auto block = co_await requestBlock(socket, hash);
        if (!block) co_return false; // Peer doesnt have block

        // Write block file
        writeFileTrunc(getTmpBlockPath(hash), serialiseBlock(*block));
    }

    // Verify blocks
    auto readTmpBlockFile = [&getTmpBlockPath](const Array256_t& hash) -> ChainBlock
    {
        auto blockBytes = readFile(getTmpBlockPath(hash));
        return parseBlock(blockBytes);
    };

    // Transactions context
    std::unordered_set<TxInput, TxInputKeyHash, TxInputKeyEq> seenUtxosInDb;
    std::unordered_set<TxInput, TxInputKeyHash, TxInputKeyEq> includeUtxos;
    VerifyTxContext txCtx;
    txCtx.seenUtxos = &seenUtxosInDb;
    txCtx.includeUtxos = &includeUtxos;

    // Verify first block
    VerifyBlockContext b0Ctx;
    b0Ctx.headerCtx = h0Ctx;
    b0Ctx.txCtx = txCtx;

    if (!verifyBlock(readTmpBlockFile(blockHashes[0]), b0Ctx)) co_return false;

    // Verify 2nd block if size > 1
    VerifyBlockContext b1Ctx;
    b1Ctx.headerCtx = h1Ctx;
    b1Ctx.txCtx = txCtx;

    if (headers.size() > 1) { if (!verifyBlock(readTmpBlockFile(blockHashes[1]), b1Ctx)) co_return false; }

    // Verify all other blocks if size > 2
    if (headers.size() > 2)
    {
        for (size_t i = 2; i < headers.size(); ++i)
        {
            VerifyBlockContext blockCtx;
            // Header
            blockCtx.headerCtx.prevHeader = &headers[i - 1];
            blockCtx.headerCtx.prevPrevTimestamp = &headers[i - 2].timestamp;
            // Transactions
            blockCtx.txCtx = txCtx;
            if (!verifyBlock(readTmpBlockFile(blockHashes[i]), blockCtx)) co_return false;
        }
    }

    // Verification complete now add to local chain
    // Undo chain up to common ancestor
    while (tipHash != commonAncestorHash)
    {
        undoNewTipBlock();
    }

    // Add new blocks from peer
    for (auto& hash : blockHashes)
    {
        addNewTipBlock(readTmpBlockFile(hash));
    }
    co_return true;
}

// ============================================
// Update chain and connect to network
// ============================================
asio::awaitable<void> trySyncWithPeers()
{
    for (auto& peer : knownPeers)
    {
        asio::ip::tcp::socket socket(ioCtx);

        try
        {
            co_await socket.async_connect(peer.first, asio::use_awaitable);

            if (!co_await requestPing(socket)) continue;
            if (!co_await syncIfBetter(socket)) continue;

            break; // synced successfully
        }
        catch (...)
        {
        }
    }
}

// ============================================
// Broadcast
// ============================================

asio::awaitable<void> BroadcastNewTx(const Tx& tx)
{
    for (const auto& peer : knownPeers)
    {
        if (peer.second.relay == 0) co_return;

        try
        {
            asio::ip::tcp::socket socket(ioCtx);
            co_await socket.async_connect(peer.first);

            // Send message type
            auto msgType = ProtocolMessage::BroadcastNewTx;
            co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

            auto txBytes = serialiseTx(tx);

            // Send transaction size
            BytesBuffer txSizeBuf;
            txSizeBuf.writeU64(txBytes.size());
            co_await asio::async_write(socket, asio::buffer(txSizeBuf.data(), txSizeBuf.size()), asio::use_awaitable);

            // Send transaction
            co_await asio::async_write(socket, asio::buffer(txBytes.data(), txBytes.size()), asio::use_awaitable);
        }
        catch (...)
        {
        }
    }
}

asio::awaitable<void> BroadcastNewBlock(const ChainBlock& block)
{
    for (const auto& peer : knownPeers)
    {
        if (peer.second.relay == 0) co_return;
        try
        {
            asio::ip::tcp::socket socket(ioCtx);
            co_await socket.async_connect(peer.first);


            // Send message type
            auto msgType = ProtocolMessage::BroadcastNewBlock;
            co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

            // Send block size
            const auto blockBytes = serialiseBlock(block);
            co_await writeU64Tcp(socket, blockBytes.size());

            // Send block
            co_await asio::async_write(socket, asio::buffer(blockBytes.data(), blockBytes.size()), asio::use_awaitable);
        }
        catch (...)
        {
        }
    }
}
