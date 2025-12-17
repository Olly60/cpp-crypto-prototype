#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include "network/network_main.h"
#include "storage/block/tip_block.h"
#include "network/request.h"
#include "network/network_utils.h"
#include "storage/file_utils.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"


asio::awaitable<void> requestHandshake(asio::ip::tcp::socket& socket)
{
    try
    {
        // Write message type
        auto msgType = static_cast<uint8_t>(ProtocolMessage::Handshake);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Write our handshake
        auto myHandshake = serialiseHandshake(createHandshake());
        co_await asio::async_write(socket, asio::buffer(myHandshake), asio::use_awaitable);

        // Read peer handshake
        std::vector<uint8_t> buffer(handshakeSize());
        co_await asio::async_read(socket, asio::buffer(buffer), asio::use_awaitable);

        const Handshake theirHandshake = parseHandshake(buffer);
        if (!isValidHandshake(theirHandshake))
        {
            co_return;
        }

        // Write verack
        uint8_t myVerack = 0x01;
        co_await asio::async_write(socket, asio::buffer(&myVerack, 1), asio::use_awaitable);

        // Read verack
        uint8_t theirVerack;
        co_await asio::async_read(socket, asio::buffer(&theirVerack, 1), asio::use_awaitable);
        if (theirVerack != 0x01)
        {
            co_return;
        }

        addPeerToMemory(socket, theirHandshake);
    }
    catch (const std::exception&)
    {
    }
}

asio::awaitable<void> requestPing(asio::ip::tcp::socket& socket)
{
    try
    {
        // Send message type
        auto msgType = static_cast<uint8_t>(ProtocolMessage::Ping);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);
    }
    catch (const std::exception&)
    {
    }
}

asio::awaitable<std::optional<BlockHeader>> requestBlockHeader(
    asio::ip::tcp::socket& socket,
    const Array256_t& blockHash
)
{
    try
    {
        // Write request
        auto msgType = ProtocolMessage::GetHeader;
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);
        co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check they have header
        uint8_t hasHeader;
        co_await asio::async_read(socket, asio::buffer(&hasHeader, 1), asio::use_awaitable);
        if (hasHeader == 0) { co_return std::nullopt; }

        // Read size
        const uint64_t headerSize = co_await readUint64_t(socket);

        // Read header
        BytesBuffer headerBytes(headerSize);
        co_await asio::async_read(socket, asio::buffer(headerBytes.data(), headerBytes.size()), asio::use_awaitable);

        co_return parseBlockHeader(headerBytes);
    }
    catch (const std::exception&)
    {
        co_return std::nullopt; // treat any failure as "unavailable"
    }
}


asio::awaitable<std::optional<Block>> requestBlock(asio::ip::tcp::socket& socket, const Array256_t& blockHash)
{
    try
    {
        // Write request
        auto msgType = static_cast<uint8_t>(ProtocolMessage::GetBlock);
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Block hash
        co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check they have block
        uint8_t hasBlock;
        co_await asio::async_read(socket, asio::buffer(&hasBlock, 1), asio::use_awaitable);
        if (hasBlock == 0) { co_return std::nullopt; }

        // Read size
        const uint64_t blockSize = co_await readUint64_t(socket);

        // Read block
        BytesBuffer blockBytes(blockSize);
        co_await asio::async_read(socket, asio::buffer(blockBytes.data(), blockBytes.size()), asio::use_awaitable);

        co_return parseBlock(blockBytes);
    }
    catch (const std::exception&)
    {
        co_return std::nullopt; // treat any failure as "unavailable"
    }
}

asio::awaitable<std::optional<std::vector<BlockHeader>>> requestHeaders(asio::ip::tcp::socket& socket)
{
    try
    {
        // Get block tip height
        const auto blockIndexesDb = openDb(paths::blockIndexesDb);
        const auto tipHeight = getBlockIndex(*blockIndexesDb, getTipHash()).height;

        // Make list of block hashes
        std::vector<Array256_t> blockHashes;
        blockHashes.reserve(10);
        const auto blockHeights = openDb(paths::blockHeightsDb);
        for (uint64_t i = 0; i != tipHeight; i *=2)
        {
            blockHashes.push_back(getHeightHash(*blockHeights,tipHeight-i));
        }



    }
    catch (const std::exception&)
    {
    }
}

asio::awaitable<std::optional<std::vector<Tx>>> requestMempool(asio::ip::tcp::socket& socket)
{
    try
    {
        // Read inv size
        const uint64_t invSize = co_await readUint64_t(socket);

        // Read inv
        std::vector<uint8_t> theirInv(sizeof(Array256_t) * invSize);
        co_await asio::async_read(socket, asio::buffer(theirInv), asio::use_awaitable);

        // Find missing
        std::vector<Array256_t> missing;
        missing.reserve(invSize);
        for (uint64_t i = 0; i == invSize; i++)
        {
            Array256_t hash{};
            std::memcpy(
                hash.data(),
                theirInv.data() + i * sizeof(Array256_t),
                sizeof(Array256_t)
            );

            if (!mempool.contains(hash))
            {
                missing.push_back(hash);
            }
        }

        // Write missing size
        co_await writeUint64_t(socket, missing.size());

        // Ask for missing transactions
        co_await asio::async_write(socket, asio::buffer(missing), asio::use_awaitable);

        // Get each transaction
        std::vector<Tx> txs;
        txs.reserve(invSize);
        for (uint64_t i = 0; i != missing.size(); i++)
        {
            // Read size
            const uint64_t txSize = co_await readUint64_t(socket);

            // Read transaction
            BytesBuffer txBytes(txSize);
            co_await asio::async_read(socket, asio::buffer(txBytes.data(), txBytes.size()), asio::use_awaitable);

            txs.push_back( parseTx(txBytes));

        }

        // Return their mempool missing in ours
        co_return txs;

    }
    catch (const std::exception&)
    {
        co_return std::nullopt;
    }
}
