#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include "network/network_main.h"
#include "storage/block/tip_block.h"
#include "network/handle.h"
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
        co_await writeUint64_t(socket, headerSize);

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
        const uint64_t blockSize = blockData.size();
        writeUint64_t(socket)

        // Send block
        co_await asio::async_write(socket, asio::buffer(blockData), asio::use_awaitable);
    }
    catch (const std::exception&)
    {
        // Failed to send block
    }
}

asio::awaitable<void> handleNewBlock(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read size
        uint64_t blockSize;
        std::array<uint8_t, 8> sizeBuf{};
        co_await asio::async_read(socket, asio::buffer(sizeBuf), asio::use_awaitable);
        takeBytesInto(blockSize, sizeBuf);

        // Read block
        std::vector<uint8_t> blockData(blockSize);
        co_await asio::async_read(socket, asio::buffer(blockData), asio::use_awaitable);

        if (const Block newBlock = parseBlock(blockData); verifyBlock(newBlock))
        {
            addBlock(newBlock);
            // TODO: broadcastBlockToPeers would need to be implemented
        }
        else if (!blockExists(getBlockHash(newBlock)))
        {
            throw std::runtime_error("Invalid block");
        }
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

asio::awaitable<bool> handleHandshake(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read peer handshake
        std::vector<uint8_t> buffer(handshakeSize());
        co_await asio::async_read(socket, asio::buffer(buffer), asio::use_awaitable);


        const Handshake theirHandshake = parseHandshake(buffer);
        if (!isValidHandshake(theirHandshake))
        {
            co_return false;
        }

        // Send our handshake
        auto myHandshake = serialiseHandshake(createHandshake());
        co_await asio::async_write(socket, asio::buffer(myHandshake), asio::use_awaitable);

        // Read verack
        uint8_t theirVerack;
        co_await asio::async_read(socket, asio::buffer(&theirVerack, 1), asio::use_awaitable);
        if (theirVerack != 0x01)
        {
            co_return false;
        }

        // Send verack
        uint8_t myVerack = 0x01;
        co_await asio::async_write(socket, asio::buffer(&myVerack, 1), asio::use_awaitable);

        co_return true;
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
        uint8_t pong = 0x01;
        co_await asio::async_write(socket, asio::buffer(&pong, 1), asio::use_awaitable);
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
        const uint64_t mempoolSize = mempool.size();
        std::vector<uint8_t> sizeBuf;
        appendBytes(sizeBuf, mempoolSize);
        co_await asio::async_write(socket, asio::buffer(sizeBuf), asio::use_awaitable);

        // Send inv
        for (const auto& [key, _] : mempool)
        {
            co_await asio::async_write(socket, asio::buffer(key), asio::use_awaitable);
        }

        // Read missing size
        uint64_t peerMissingSize;
        std::array<uint8_t, 8> peerMissingSizeBuf{};
        co_await asio::async_read(socket, asio::buffer(peerMissingSizeBuf), asio::use_awaitable);
        takeBytesInto(peerMissingSize, peerMissingSizeBuf);


        // Read missing
        std::vector<Array256_t> peerMissing;
        peerMissing.reserve(peerMissingSize);
        for (uint64_t i = 0; i < peerMissingSize; i++)
        {
            Array256_t peerMissingHash;
            co_await asio::async_read(socket, asio::buffer(peerMissingHash), asio::use_awaitable);
            peerMissing.push_back(peerMissingHash);
        }

        /////////////////////////////////////////////

        // Get each transaction
        std::vector<Tx> txs;
        txs.reserve(invSize);
        for (uint64_t i = 0; i != missing.size(); i++)
        {
            // Read size
            uint64_t txSize;
            std::array<uint8_t, 8> txSizeBuf{};
            co_await asio::async_read(socket, asio::buffer(txSizeBuf), asio::use_awaitable);
            takeBytesInto(txSize, txSizeBuf);

            // Read transaction
            std::vector<uint8_t> txBytes(txSize);
            co_await asio::async_read(socket, asio::buffer(txBytes), asio::use_awaitable);

            txs.push_back(parseTx(txBytes));
        }
        catch
        (const std::exception&)
        {
        }
    }
}
