#include "storage/peers.h"

#include "network/network_main.h"
#include "storage/storage_utils.h"

void storePeers()
{
    BytesBuffer knownPeersFileBytes;

    // Write peer count
    knownPeersFileBytes.writeU64(knownPeers.size());

    // Write each peer
    for (const auto& [peerAddr, peerStatus] : knownPeers)
    {
        // Write address type
        knownPeersFileBytes.writeU8(peerAddr.address.is_v4() ? 0x04 : 0x06);

        // Address
        if (peerAddr.address.is_v4())
        {
            knownPeersFileBytes.writeFixedArray(peerAddr.address.to_v4().to_bytes());
        } else if (peerAddr.address.is_v6()) {
            knownPeersFileBytes.writeFixedArray(peerAddr.address.to_v6().to_bytes());
        }


        // Port
        knownPeersFileBytes.writeU16(peerAddr.port);

        // Services
        knownPeersFileBytes.writeU64(peerStatus.services);

        // Relay
        knownPeersFileBytes.writeU8(peerStatus.relay);

        // Last seen
        knownPeersFileBytes.writeU64(peerStatus.lastSeen);
    }

    writeFileTrunc(KNOWN_PEERS, knownPeersFileBytes);

    BytesBuffer unknownPeersFileBytes;

    // Write Peer count
    unknownPeersFileBytes.writeU64(unknownPeers.size());

    // Write each peer
    for (const auto& peerAddr : unknownPeers)
    {
        // Address length and string
        unknownPeersFileBytes.writeString(peerAddr.address.to_string());

        // Port
        unknownPeersFileBytes.writeU16(peerAddr.port);
    }

    writeFileTrunc(UNKNOWN_PEERS, unknownPeersFileBytes);
}

void loadPeers()
{
    if (std::filesystem::exists(KNOWN_PEERS))
    {
        auto knownPeersFileBytes = readFile(KNOWN_PEERS);

        // Read peer count
        uint64_t peersCount = knownPeersFileBytes->readU16();

        // Read each peer
        for (uint64_t i = 0; i < peersCount; ++i)
        {
            PeerAddress peerAddr;
            PeerStatus peerStatus;

            // Read address type
            uint8_t addressType = knownPeersFileBytes->readU8();

            // Read address
            if (addressType == 4)
            {

                peerAddr.address = asio::ip::address_v4(knownPeersFileBytes->readFixedArray<4>());
            } else if (addressType == 6)
            {
                peerAddr.address = asio::ip::address_v6(knownPeersFileBytes->readFixedArray<16>());
            }

            // Read port
            peerAddr.port = knownPeersFileBytes->readU16();

            // Read services
            peerStatus.services = knownPeersFileBytes->readU64();

            // Read last seen
            peerStatus.lastSeen = knownPeersFileBytes->readU64();

            // Read relay
            peerStatus.relay = knownPeersFileBytes->readU8();

            knownPeers.insert({peerAddr, peerStatus});
        }
    }
    if (std::filesystem::exists(UNKNOWN_PEERS))
    {
        auto unknownPeersFileBytes = readFile(UNKNOWN_PEERS);

        // Read peer count
        uint64_t peersCount = unknownPeersFileBytes->readU64();

        // Read each peer
        for (uint64_t i = 0; i < peersCount; ++i)
        {
            PeerAddress peerAddr;

            // Read address type
            uint8_t addressType = unknownPeersFileBytes->readU8();

            // Read address
            if (addressType == 4)
            {

                peerAddr.address = asio::ip::address_v4(unknownPeersFileBytes->readFixedArray<4>());
            } else if (addressType == 6)
            {
                peerAddr.address = asio::ip::address_v6(unknownPeersFileBytes->readFixedArray<16>());
            }

            // Read port
            peerAddr.port = unknownPeersFileBytes->readU16();

            unknownPeers.insert(peerAddr);
        }

    }
}
