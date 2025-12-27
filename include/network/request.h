#pragma once
#include <map>

asio::awaitable<void> requestHandshake(asio::ip::tcp::socket& socket);

asio::awaitable<void> requestPing(asio::ip::tcp::socket& socket);

asio::awaitable<std::optional<BlockHeader>> requestBlockHeader(
    asio::ip::tcp::socket& socket,
    const Array256_t& blockHash
);

asio::awaitable<std::optional<Block>> requestBlock(asio::ip::tcp::socket& socket, const Array256_t& blockHash);

asio::awaitable<std::map<Array256_t, BlockHeader>> requestHeaders(asio::ip::tcp::socket& socket);

asio::awaitable<void> requestMempool(asio::ip::tcp::socket& socket);