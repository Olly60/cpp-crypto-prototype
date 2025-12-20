#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include "network/network_main.h"
#include "storage/block/tip_block.h"
#include "network/handle.h"
#include <ranges>
#include "network/network_utils.h"
#include "storage/block/block_utils.h"

asio::awaitable<void> handleGetHeader(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read block hash
        Array256_t blockHash;
        co_await asio::async_read(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check header is in storage
        if (!blockExists(blockHash))
        {
            uint8_t haveHeader = 0;
            co_await asio::async_write(socket, asio::buffer(&haveHeader, 1), asio::use_awaitable);
            co_return;
        }

        // Tell peer I have it
        uint8_t haveBlock = 1;
        co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);

        // Get header from storage
        auto headerBytes = readBlockFileHeaderBytes(blockHash);

        // Send header
        co_await asio::async_write(socket, asio::buffer(headerBytes.data(), headerBytes.size()), asio::use_awaitable);

    }
    catch (const std::exception&)
    {
        // Failed to send header
    }
}

asio::awaitable<void> handleGetBlock(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read block hash
        Array256_t blockHash;
        co_await asio::async_read(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check block is in storage
        if (!blockExists(blockHash))
        {
            // Write I dont have it
            uint8_t haveBlock = 0;
            co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);
            co_return;
        }

        // Write have it
        uint8_t haveBlock = 1;
        co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);

        // Get block from storage
        auto blockBytes = readBlockFileBytes(blockHash);

        // Write size
        BytesBuffer blockSize(blockBytes.size());
        co_await asio::async_write(socket, asio::buffer(blockSize.data(), blockSize.size()), asio::use_awaitable);

        // Write block
        co_await asio::async_write(socket, asio::buffer(blockBytes.data(), blockBytes.size()), asio::use_awaitable);
    }
    catch (const std::exception&)
    {
        // Failed to send block
    }
}

asio::awaitable<void> handleHandshake(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read peer handshake
        BytesBuffer buffer(handshakeSize());
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

        addPeerToMemory(socket, theirHandshake);

    }
    catch (const std::exception&)
    {
        // Connection failed
    }
}

asio::awaitable<void> handlePing(asio::ip::tcp::socket& socket)
{
    try
    {
        constexpr uint8_t pong = 0x01;
        co_await asio::async_write(socket, asio::buffer(&pong ,1), asio::use_awaitable);
    }
    catch (const std::exception&)
    {
        // Failed to respond
    }
}

asio::awaitable<void> handleGetHeaders(asio::ip::tcp::socket& socket)
{
    try
    {
        //TODO: make function
    }
    catch (const std::exception&)
    {
    }
}

asio::awaitable<void> handleGetMempool(asio::ip::tcp::socket& socket)
{
    try
    {
        // Write size
        BytesBuffer mempoolSizeBuf(mempool.size());
        co_await asio::async_write(socket, asio::buffer(mempoolSizeBuf.data(), mempoolSizeBuf.size()), asio::use_awaitable);

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
            co_await asio::async_write(socket, asio::buffer(peerMissingTxBytes.data(), peerMissingTxBytes.size()), asio::use_awaitable);
        }

        }
        catch
        (const std::exception&)
        {
        }
    }

// ============================================
// Handle new data
// ============================================

asio::awaitable<void> handleNewBlock(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read size
        const uint64_t blockSize = co_await readU64Tcp(socket);

        // Read block
        BytesBuffer blockData(blockSize);
        co_await asio::async_read(socket, asio::buffer(blockData.data(), blockData.size()), asio::use_awaitable);

        // TODO: process block
    }
    catch (const std::exception&)
    {
        // Failed to process block
    }
}

asio::awaitable<void> handleNewTx(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read size
        const uint64_t txSize = co_await readU64Tcp(socket);

        // Read transaction
        BytesBuffer txBytes(txSize);
        co_await asio::async_read(socket, asio::buffer(txBytes.data(), txBytes.size()), asio::use_awaitable);

        //TODO: process tx
    }
    catch (const std::exception&)
    {
        // Failed to process transaction
    }
}