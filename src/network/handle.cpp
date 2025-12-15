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
#include "block_verification.h"

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

        // Send size
        const uint64_t headerSize = headerBytes.size();
        co_await writeNumber(socket, headerSize);

        // Send header
        co_await asio::async_write(socket, asio::buffer(headerBytes), asio::use_awaitable);
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
            uint8_t haveBlock = 0;
            co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);
            co_return;
        }
        // Tell peer I have it
        uint8_t haveBlock = 1;
        co_await asio::async_write(socket, asio::buffer(&haveBlock, 1), asio::use_awaitable);

        // Get block from storage
        auto blockData = readBlockFileBytes(blockHash);

        // Send size
        co_await writeNumber(socket, blockData.size());

        // Send block
        co_await asio::async_write(socket, asio::buffer(blockData), asio::use_awaitable);
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
        std::vector<uint8_t> buffer(handshakeSize());
        co_await asio::async_read(socket, asio::buffer(buffer), asio::use_awaitable);

        const auto theirHandshake = parseHandshake(buffer);
        // Is handshake valid
        if (!isValidHandshake(theirHandshake))
        {
            co_return;
        }

        // Send our handshake
        auto myHandshake = serialiseHandshake(createHandshake());
        co_await asio::async_write(socket, asio::buffer(myHandshake), asio::use_awaitable);

        // Read verack
        if (const uint8_t theirVerack = co_await readNumber<uint8_t>(socket); theirVerack != 0x01)
        {
            co_return;
        }

        // Send verack
        constexpr uint8_t myVerack = 0x01;
        co_await writeNumber(socket, myVerack);


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
        co_await writeNumber(socket, pong);
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
        // Send size
        co_await writeNumber(socket, mempool.size());

        // Send inv
        for (const auto& key : mempool | std::views::keys)
        {
            co_await asio::async_write(socket, asio::buffer(key), asio::use_awaitable);
        }

        // Read missing size
        const uint64_t peerMissingSize = co_await readNumber<uint64_t>(socket);

        // Read missing hashes
        std::vector<Array256_t> peerMissingHashes;
        peerMissingHashes.reserve(peerMissingSize);
        for (uint64_t i = 0; i < peerMissingSize; i++)
        {
            Array256_t peerMissingHash;
            co_await asio::async_read(socket, asio::buffer(peerMissingHash), asio::use_awaitable);
            peerMissingHashes.push_back(peerMissingHash);

        }

        // Send missing transactions
        std::vector<Tx> peerMissingTxs;
        peerMissingTxs.reserve(peerMissingSize);

        for (const auto key : peerMissingHashes)
        {
            const auto peerMissingTxBytes = serialiseTx(mempool[key]);

            // Send transaction size
            co_await writeNumber(socket, peerMissingTxBytes.size());

            // Send transaction
            co_await asio::async_write(socket, asio::buffer(peerMissingTxBytes), asio::use_awaitable);
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
        const uint64_t blockSize = co_await readNumber<uint64_t>(socket);

        // Read block
        std::vector<uint8_t> blockData(blockSize);
        co_await asio::async_read(socket, asio::buffer(blockData), asio::use_awaitable);

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
        //TODO: make function
    }
    catch (const std::exception&)
    {
        // Failed to process transaction
    }
}