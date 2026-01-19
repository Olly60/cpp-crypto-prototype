#include "storage/peers.h"

#include "network/network_main.h"
#include "storage/storage_utils.h"

void storePeers()
{
    BytesBuffer knownPeersFileBytes;

    // Write peer count
    knownPeersFileBytes.writeU64(knownPeers.size());

    // Write each peer
    for (const auto& peer : knownPeers)
    {
        // Write address type
        knownPeersFileBytes.writeU8(peer.first.is_v4() ? 0x04 : 0x06);

        // Address
        if (peer.first.is_v4())
        {
            knownPeersFileBytes.writeFixedArray(peer.first.to_v4().to_bytes());
        } else if (peer.first.is_v6()) {
            knownPeersFileBytes.writeFixedArray(peer.first.to_v6().to_bytes());
        }

        // Port
        knownPeersFileBytes.writeU16(peer.second.port);

        // Services
        knownPeersFileBytes.writeU64(peer.second.services);

        // Relay
        knownPeersFileBytes.writeU8(peer.second.relay);

        // Last seen
        knownPeersFileBytes.writeU64(peer.second.lastSeen);
    }

    writeFileTrunc(KNOWN_PEERS, knownPeersFileBytes.data(), knownPeersFileBytes.size());

    BytesBuffer unknownPeersFileBytes;

    // Write Peer count
    unknownPeersFileBytes.writeU64(unknownPeers.size());

    // Write each peer
    for (const auto& peerAddr : unknownPeers)
    {
        // Write address type
        unknownPeersFileBytes.writeU8(peerAddr.address().is_v4() ? 0x04 : 0x06);

        // Address
        if (peerAddr.address().is_v4())
        {
            unknownPeersFileBytes.writeFixedArray(peerAddr.address().to_v4().to_bytes());
        } else if (peerAddr.address().is_v6()) {
            unknownPeersFileBytes.writeFixedArray(peerAddr.address().to_v6().to_bytes());
        }

        // Port
        unknownPeersFileBytes.writeU16(peerAddr.port());
    }

    writeFileTrunc(UNKNOWN_PEERS, unknownPeersFileBytes.data(), unknownPeersFileBytes.size());
}

void loadPeers()
{
    // Known peers
    if (auto knownPeersFileBytes = readFile(KNOWN_PEERS))
    {
        uint64_t peersCount = knownPeersFileBytes->readU64(); // must match storePeers()

        for (uint64_t i = 0; i < peersCount; ++i)
        {
            asio::ip::address peerAddr;
            PeerStatus peerStatus;

            uint8_t addressType = knownPeersFileBytes->readU8();

            // Read address
            if (addressType == 4)
                peerAddr = asio::ip::address_v4(knownPeersFileBytes->readFixedArray<4>());
            else if (addressType == 6)
                peerAddr = asio::ip::address_v6(knownPeersFileBytes->readFixedArray<16>());

            // Read fields in the order they were written
            peerStatus.port = knownPeersFileBytes->readU16();
            peerStatus.services = knownPeersFileBytes->readU64();
            peerStatus.relay = knownPeersFileBytes->readU8();
            peerStatus.lastSeen = knownPeersFileBytes->readU64();

            knownPeers.insert({peerAddr, peerStatus});
        }
    }

    // Unknown peers
    if (auto unknownPeersFileBytes = readFile(UNKNOWN_PEERS))
    {
        uint64_t peersCount = unknownPeersFileBytes->readU64();

        for (uint64_t i = 0; i < peersCount; ++i)
        {
            asio::ip::address addr;
            uint8_t addressType = unknownPeersFileBytes->readU8();

            if (addressType == 4)
                addr = asio::ip::address_v4(unknownPeersFileBytes->readFixedArray<4>());
            else if (addressType == 6)
                addr = asio::ip::address_v6(unknownPeersFileBytes->readFixedArray<16>());

            uint16_t port = unknownPeersFileBytes->readU16();
            unknownPeers.insert(asio::ip::tcp::endpoint(addr, port));
        }
    }
}


asio::ip::address normalizeAddress(asio::ip::address addr)
{
    if (addr.is_v6() && addr.to_v6().is_v4_mapped())
        return addr.to_v6().to_v4();
    return addr;
}
