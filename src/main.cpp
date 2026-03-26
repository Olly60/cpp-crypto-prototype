#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/ip/tcp.hpp>
#include <iostream>
#include <thread>

#include "network/network_main.h"
#include "storage/peers.h"
#include "node.h"
#include "user_commands.h"
#include "wallet.h"

int main(int argc, char* argv[])
{

    // If a program argument is given, parse it as the port
    if (argc >= 2)
    {
        try
        {
            localPort = std::stoi(argv[1]);
        }
        catch (...)
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

    std::cout << "Type 'help' for a list of commands\n";

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
