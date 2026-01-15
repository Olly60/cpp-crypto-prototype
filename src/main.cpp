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
#include "../include/block.h"

// ============================================
// Main
// ============================================

int main()
{

    initGenesisBlock(); // Add genisis if first time loading

    loadPeers(); // Load peers into memory

    // Sync to latest chain
    asio::co_spawn(ioCtx, trySyncWithPeers(), asio::use_future);
    ioCtx.run();
    ioCtx.restart();


    std::cout << "Enter port: ";
    std::string port;
    std::getline(std::cin, port);
    uint16_t portNum = std::stoi(port);

    asio::co_spawn(ioCtx, acceptConnections(portNum), asio::detached);

    std::thread handlePeers([](){ ioCtx.run();});

    while (true)
    {
        // Handle user input
        std::string input;
        std::getline(std::cin ,input);
        if (input == "exit") break;
        handleUserCommand(input);
    }

    ioCtx.stop();
    handlePeers.join();
    storePeers();
}
