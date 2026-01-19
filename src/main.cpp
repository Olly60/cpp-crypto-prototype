#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/ip/tcp.hpp>
#include "network/network_main.h"
#include "storage/peers.h"
#include <iostream>
#include <asio/use_future.hpp>
#include <thread>
#include "node.h"
#include "user_commands.h"
#include "block.h"
#include "block_work.h"
#include "wallet.h"

// ============================================
// Main
// ============================================

int main(int argc, char* argv[])
{

    // If a program argument is given, parse it as the port
    if (argc >= 2)
    {
        try
        {
            localPort = std::stoi(argv[1]);
        }
        catch (const std::exception& e)
        {
            std::cerr << "Invalid port argument, using default 50000\n";
        }
    }

    std::cout << "Using port: " << localPort << "\n";

    initGenesisBlock();
    loadPeers();
    loadWallets();

    // Sync to latest chain
    asio::co_spawn(ioCtx, trySyncWithPeers(), asio::detached);
    ioCtx.run();
    ioCtx.restart();

    asio::co_spawn(ioCtx, acceptConnections(localPort), asio::detached);

    std::jthread handlePeers([](){ ioCtx.run();});

    while (true)
    {
        // Handle user input
        std::string input;
        std::getline(std::cin ,input);
        if (input == "exit") break;
        handleUserCommand(input);
    }

    // Save
    ioCtx.stop();
    storePeers();
    storeWallets();
}
