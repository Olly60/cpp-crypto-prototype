#include <asio.hpp>
#include <asio/awaitable.hpp>
#include <asio/use_awaitable.hpp>
#include "crypto_utils.h"
#include "network/network_main.h"
#include "../../include/tip.h"
#include "network/request.h"
#include "network/network_utils.h"
#include "storage/storage_utils.h"
#include "storage/block/block_heights.h"
#include "storage/block/block_indexes.h"
#include "storage/block/block_utils.h"


asio::awaitable<void> requestHandshake(asio::ip::tcp::socket& socket)
{
    try
    {
        // Write message type
        auto msgType = ProtocolMessage::Handshake;
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Write our handshake
        auto myHandshake = serialiseHandshake(createHandshake());
        co_await asio::async_write(socket, asio::buffer(myHandshake.data(), myHandshake.size()), asio::use_awaitable);

        // Read peer handshake
        BytesBuffer buffer(handshakeSize());
        co_await asio::async_read(socket, asio::buffer(buffer.data(), buffer.size()), asio::use_awaitable);

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

        // Write header hash
        co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check they have header
        uint8_t hasHeader;
        co_await asio::async_read(socket, asio::buffer(&hasHeader, 1), asio::use_awaitable);
        if (hasHeader == 0) { co_return std::nullopt; }

        // Read size
        const uint64_t headerSize = co_await readU64Tcp(socket);

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
        auto msgType = ProtocolMessage::GetBlock;
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Block hash
        co_await asio::async_write(socket, asio::buffer(blockHash), asio::use_awaitable);

        // Check they have block
        uint8_t hasBlock;
        co_await asio::async_read(socket, asio::buffer(&hasBlock, 1), asio::use_awaitable);
        if (hasBlock == 0) { co_return std::nullopt; }

        // Read size
        const uint64_t blockSize = co_await readU64Tcp(socket);

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

asio::awaitable<std::vector<BlockHeader>> requestHeaders(asio::ip::tcp::socket& socket)
{
    try
    {
        // Write message type
        auto msgType = ProtocolMessage::GetHeaders;
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Get block tip height
        auto tipHeight = getTipHeight();

        // Make list of block hashes with (Ancestor -> Tip)
        std::vector<Array256_t> blockHashes;
        auto blockIndexesDb = openBlockIndexesDb();

        // Add genesis block hash at the start
        blockHashes.push_back(getGenesisBlockHash());

        // Exponential hash collection (Ancestor -> Tip)
        for (uint64_t i = 1; i < tipHeight; i++) {
            blockHashes.push_back(*tryGetHeightHash(*blockIndexesDb, i));
        }

        // Add tip block hash at the end
        blockHashes.push_back(getTipHash());

        // Write amount of hashes
        co_await writeU64Tcp(socket, blockHashes.size());

        // Write hashes
        for (const auto& hash : blockHashes)
        {
            co_await asio::async_write(socket, asio::buffer(hash), asio::use_awaitable);
        }

        // Read header amount
        uint64_t headerAmount = co_await readU64Tcp(socket);

        // Read headers
        std::vector<BlockHeader> headers;
        for (uint64_t i = 0; i < headerAmount; i++) {
            BytesBuffer headerBytes(calculateBlockHeaderSize());
            co_await asio::async_read(socket, asio::buffer(headerBytes.data(), headerBytes.size()), asio::use_awaitable);
            headers.push_back(parseBlockHeader(headerBytes));
        }

        // Drop leading headers we already have (chronological order: oldest -> newest)
        std::vector<BlockHeader>::difference_type drop = 0;
        Array256_t commonAncestor;

        while (drop < static_cast<std::vector<BlockHeader>::difference_type>(headers.size()) &&
               std::filesystem::exists(getBlockFilePath(getBlockHeaderHash(headers[drop]))))
        {
            ++drop;
        }

        if (drop > 0)
        {
            headers.erase(headers.begin(), headers.begin() + drop);
        }


        co_return headers;
    }
    catch (const std::exception&)
    {
        co_return std::vector<BlockHeader>{};
    }
}

asio::awaitable<std::vector<Tx>> requestMempool(asio::ip::tcp::socket& socket)
{
    try
    {
        // Write message type
        auto msgType = ProtocolMessage::GetMempool;
        co_await asio::async_write(socket, asio::buffer(&msgType, 1), asio::use_awaitable);

        // Read inv size
        const uint64_t invSize = co_await readU64Tcp(socket);

        // Read inv
        std::vector<Array256_t> theirInv(invSize);

        for (uint64_t i = 0; i < invSize; i++)
        {
            Array256_t hash;
            co_await asio::async_read(socket, asio::buffer(hash), asio::use_awaitable);
            theirInv.push_back(hash);
        }

        // Find missing
        std::vector<Array256_t> missingInv;
        missingInv.reserve(invSize);
        for (auto& hash : theirInv){

            if (!mempool.contains(hash))
            {
                missingInv.push_back(hash);
            }
        }

        // Write missing size
        co_await writeU64Tcp(socket, missingInv.size());

        // Ask for missing transactions
        for (const auto& hash : missingInv)
        {
            co_await asio::async_write(socket, asio::buffer(hash), asio::use_awaitable);
        }

        // Get each transaction
        std::vector<Tx> txs;
        txs.reserve(invSize);
        for (uint64_t i = 0; i != missingInv.size(); i++)
        {
            // Read size
            const uint64_t txSize = co_await readU64Tcp(socket);

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
        co_return std::vector<Tx>{};
    }
}
