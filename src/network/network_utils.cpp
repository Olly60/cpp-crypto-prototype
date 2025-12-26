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
constexpr uint64_t handshakeSize()
{
    return sizeof(decltype(Handshake::nonce)) + sizeof(decltype(Handshake::blockchainTip)) + sizeof(decltype(
            Handshake::genesisBlockHash)) + sizeof(decltype(Handshake::services)) + sizeof(decltype(Handshake::version))
        +
        sizeof(decltype(Handshake::relay));
}

Handshake createHandshake()
{
    return {
        ProtocolVersion,
        GenesisBlockHash,
        FullNode,
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
    auto headers = co_await requestHeaders(socket);

    // Headers empty
    if (headers.empty()) co_return;

    auto commonAncestorHeader = getBlockHeader(headers[0].prevBlockHash);
    if (!commonAncestorHeader) co_return; // common ancestor doesnt exist

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
    if (!isLessLE(getTipChainWork(), peerChainwork)) co_return;

    // Verify first header
    VerifyBlockHeaderContext h0Ctx;
    h0Ctx.prevHeader = &*commonAncestorHeader;
    uint64_t h0CtxTimestamp = getBlockHeader(commonAncestorHeader->prevBlockHash)->timestamp;
    h0Ctx.prevPrevTimestamp = &h0CtxTimestamp;
    if (!verifyBlockHeader(headers[0], h0Ctx)) co_return;

    // Verify 2nd header if size > 1
    VerifyBlockHeaderContext h1Ctx;
    h1Ctx.prevHeader = &headers[0];
    uint64_t h1CtxTimestamp = getBlockHeader(headers[0].prevBlockHash)->timestamp;
    h1Ctx.prevPrevTimestamp = &h1CtxTimestamp;
    if (headers.size() > 1) { if (!verifyBlockHeader(headers[1], h1Ctx)) co_return; }

    // Verify all other headers if size > 2
    if (headers.size() > 2)
    {
        for (size_t i = 2; i < headers.size(); ++i)
        {
            VerifyBlockHeaderContext headerCtx;
            headerCtx.prevHeader = &headers[i - 1];
            headerCtx.prevPrevTimestamp = &headers[i - 2].timestamp;
            if (!verifyBlockHeader(headers[i], headerCtx)) co_return;
        }
    }

    std::filesystem::path tmpBlocksPath = "tmp_blockchain";
    auto getTmpBlockPath = [&tmpBlocksPath](const Array256_t& hash) -> std::filesystem::path
    {
        BytesBuffer hashBuf;
        hashBuf.writeArray256(hash);
        return tmpBlocksPath / (bytesToHex(hashBuf) + ".block");
    };

    // If local blockchain is just genesis then verify and add and don't have a tmp chain
    auto commonAncestorHash = getBlockHeaderHash(*commonAncestorHeader);
    auto tipHash = getTipHash();
    if (commonAncestorHash == getGenesisBlockHash() && tipHash == getGenesisBlockHash())
    {
        for (auto& hash : blockHashes)
        {
            // Read block
            auto block = co_await requestBlock(socket, hash);
            if (!block) co_return; // Peer doesnt have block

            if (verifyBlock(*block))
            {
                addNewTipBlock(*block);
            }
            else
            {
                co_return;
            }
        }
    }

    // Store blocks
    for (auto& hash : blockHashes)
    {
        // Read block
        auto block = co_await requestBlock(socket, hash);
        if (!block) co_return; // Peer doesnt have block

        // Write block file
        writeFileTrunc(getTmpBlockPath(hash), serialiseBlock(*block));
    }

    // Verify blocks
    auto readTmpBlockFile = [&getTmpBlockPath](const Array256_t& hash) -> Block
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

    if (!verifyBlock(readTmpBlockFile(blockHashes[0]), b0Ctx)) co_return;

    // Verify 2nd block if size > 1
    VerifyBlockContext b1Ctx;
    b1Ctx.headerCtx = h1Ctx;
    b1Ctx.txCtx = txCtx;

    if (headers.size() > 1) { if (!verifyBlock(readTmpBlockFile(blockHashes[1]), b1Ctx)) co_return; }

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
            if (!verifyBlock(readTmpBlockFile(blockHashes[i]), blockCtx)) co_return;
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

// ============================================
// Broadcast
// ============================================

asio::awaitable<void> BroadcastNewTx(asio::io_context asioCtx, const Tx& tx)
{
    //TODO: finish making
    for (const auto& [addr, port] : peers)
    {
        asio::ip::tcp::socket socket(co_await asio::this_coro::executor);

        try
        {
            asio::ip::tcp::endpoint ep(
                asio::ip::make_address(addr.ip), addr.port);

            co_await socket.async_connect(ep, asio::use_awaitable);

            co_await asio::async_write(socket, asio::buffer(message), asio::use_awaitable);
        }
        catch (const std::exception&)
        {
        }
    }
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

asio::awaitable<void> BroadcastNewBlock(const Block& block)
{
    //TODO: finish making
    // Send message type
    auto msgType = ProtocolMessage::BroadcastNewBlock;
    co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

    // Send block size
    const auto blockBytes = serialiseBlock(block);
    co_await writeU64Tcp(socket, blockBytes.size());

    // Send block
    co_await asio::async_write(socket, asio::buffer(blockBytes.data(), blockBytes.size()), asio::use_awaitable);
}
