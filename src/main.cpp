#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/ip/tcp.hpp>
#include "network/network_main.h"
#include "storage/peers.h"
#include <thread>
#include <iostream>
#include <asio/use_future.hpp>

#include "network/request.h"

void cleanup()
{
    ioCtx.stop();
    storePeers();
}
// ============================================
// Main
// ============================================

int main()
{
    std::atexit(cleanup);

    initGenesisBlock(); // Add genisis if first time loading

    loadPeers(); // Load peers into memory

    // Sync to latest chain
    std::cout << "Syncing blockchain with peer\n";
    std::future<bool> syncResult = asio::co_spawn(ioCtx, trySyncWithPeers(), asio::use_future);

    ioCtx.run();

    // Get the result
    bool synced = syncResult.get();
    std::cout << (synced ? "Blockchain synced\n" : "Didn't sync blockchain\n");

    ioCtx.restart();

    while (true)
    {
        // Handle peers
        asio::co_spawn(ioCtx, acceptConnections(), asio::detached);

        ioCtx.poll();

        // Handle user input
        std::string input;
        std::cin >> input;


        ioCtx.poll();

    }
}

// std::vector<std::string> splitCommand(const std::string& input) {
//     std::vector<std::string> parts;
//     std::istringstream stream(input);
//     std::string part;
//     while (stream >> part) {  // reads word by word separated by whitespace
//         parts.push_back(part);
//     }
//     return parts;
// }
//
// void handleUserCommand(const std::string& cmd) {
//     auto parts = splitCommand(cmd);
//     if (parts.empty()) return;
//
//     if (parts[0] == "ping") {
//         // Spawn the coroutine without blocking
//         asio::co_spawn(ioCtx, requestPing(), asio::detached);
//     }
// }
