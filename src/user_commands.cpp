#include <vector>
#include "network/request.h"
#include "user_commands.h"
#include <iostream>
#include <asio/use_awaitable.hpp>
#include <asio/use_future.hpp>
#include <asio/co_spawn.hpp>
#include <asio/ip/tcp.hpp>
#include "node.h"
#include "tip.h"
#include "network/network_main.h"
#include "storage/block/block_indexes.h"
#include "storage/block/block_utils.h"

asio::io_context userIo;

asio::awaitable<void> handleUserNetworkCommand(const std::vector<std::string> & parts) {
    if (parts.size() < 3) co_return;
    std::cout << "awaiting response from peer...\n";

    asio::ip::tcp::socket socket(userIo);

    co_await socket.async_connect(asio::ip::tcp::endpoint(asio::ip::make_address(parts[2]), static_cast<uint16_t>(std::stoi(parts[3]))), asio::use_awaitable);

    std::cout << "connected to: " << socket.remote_endpoint().address().to_string() << "\n";

        if (parts[1] == "ping") {

            auto result = co_await requestPing(socket);
            if (result)
            {
                std::cout << "Pong from: " << socket.remote_endpoint().address().to_string() << "\n";
                co_return;
            }
                std::cout << "No pong from: " << socket.remote_endpoint().address().to_string() << "\n";
        } else if (parts[1] == "getmempool")
        {
            auto result = co_await requestMempool(socket);
            if (result)
            {
                std::cout << "Got mempool from: " << socket.remote_endpoint().address().to_string() << "\n";
                co_return;
            }
            std::cout << "Failed to get whole mempool from: " << socket.remote_endpoint().address().to_string() << "\n";
        } else if (parts[1] == "getpeers")
        {
            auto result = co_await requestPeers(socket);
            if (result)
            {
                std::cout << "Got peers from: " << socket.remote_endpoint().address().to_string() << "\n";
                co_return;
            }
            std::cout << "Failed to get peers from: " << socket.remote_endpoint().address().to_string() << "\n";
        } else if (parts[1] == "handshake")
        {
            auto result = co_await requestHandshake(socket);
            if (result)
            {
                std::cout << "Successful handshake with: " << socket.remote_endpoint().address().to_string() << "\n";
                co_return;
            }
            std::cout << "Failed to handshake with: " << socket.remote_endpoint().address().to_string() << "\n";
        } else if (parts[1] == "sync")
        {
            auto result = co_await syncIfBetter(socket);
            if (result)
            {
                std::cout << "Synced with: " << socket.remote_endpoint().address().to_string() << "\n";
                co_return;
            }
            std::cout << "Didn't sync with: " << socket.remote_endpoint().address().to_string() << "\n";
        }
}

void handleUserCommand(const std::string& input)
{
    std::vector<std::string> parts;
    std::istringstream stream(input);
    std::string part;
    while (stream >> part) {
        parts.push_back(part);
    }

    if (parts[0] == "peers")
    {
        asio::co_spawn(userIo, handleUserNetworkCommand(parts), asio::use_future);
        userIo.run();
        return;
    }

    if (parts[0] == "chaininfo")
    {
        std::cout << "Length: " << std::to_string(tryGetBlockIndex(getTipHash())->height + 1)  << "\n";
        BytesBuffer chainWorkBuf;
        chainWorkBuf.writeArray256(tryGetBlockIndex(getTipHash())->chainWork);
        std::cout << "Chain work: " << bytesToHex(chainWorkBuf) << "\n";
    }
}