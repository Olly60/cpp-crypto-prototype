#pragma once
#include <optional>
#include <asio/awaitable.hpp>
#include <asio.hpp>
#include "crypto_utils.h"

asio::awaitable<bool> requestHandshake(asio::ip::tcp::socket& socket);

asio::awaitable<bool> requestPeers(asio::ip::tcp::socket& socket);

asio::awaitable<bool> requestPing(asio::ip::tcp::socket& socket);

asio::awaitable<std::optional<ChainBlock>> requestBlock(asio::ip::tcp::socket& socket, const Array256_t& blockHash);

asio::awaitable<std::vector<BlockHeader>> requestHeaders(asio::ip::tcp::socket& socket);

asio::awaitable<bool> requestMempool(asio::ip::tcp::socket& socket);