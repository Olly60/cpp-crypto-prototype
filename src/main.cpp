#include <asio/co_spawn.hpp>
#include <asio/detached.hpp>
#include <asio/ip/tcp.hpp>
#include "network/network_main.h"
#include "storage/block/genesis_block.h"
#include "storage/peers.h"
#include "storage/block/block_heights.h"


// ============================================
// Main
// ============================================

int main()
{
    //initGenesisBlock(); // Add genisis if first time loading

    //loadPeers(); // Load peers into memory

    // Sync to latest chain
    //asio::co_spawn(ioCtx, trySyncWithPeers(), asio::detached);
    //ioCtx.run();
    //ioCtx.restart();
    // Start handling peers
    //asio::co_spawn(ioCtx, acceptConnections(), asio::detached);
    //ioCtx.run();
    //storePeers();

}
