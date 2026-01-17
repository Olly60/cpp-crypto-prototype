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
#include "wallet.h"

// ============================================
// Main
// ============================================

int main()
{

    initGenesisBlock();

    loadPeers();

    loadWallets();

    // Sync to latest chain
    asio::co_spawn(ioCtx, trySyncWithPeers(), asio::use_future);
    ioCtx.run();
    ioCtx.restart();


    std::cout << "Enter port (default is 50000): ";
    std::string port;
    std::getline(std::cin, port);
    localPort = std::stoi(port);

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
