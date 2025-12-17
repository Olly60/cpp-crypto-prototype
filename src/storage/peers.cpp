#include "storage/peers.h"
#include "storage/file_utils.h"

void storePeers(const std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash>& peers)
{
    fs::create_directories(paths::peers.parent_path());

    std::ofstream peersFile(paths::peers, std::ios::trunc | std::ios::binary);
    if (!peersFile)
    {
        throw std::runtime_error("Failed to open peers file for writing");
    }
    peersFile.exceptions(std::ios::failbit | std::ios::badbit);

    BytesBuffer peersFileBytes;

    // Write peer count
    peersFileBytes << uint64_t{peers.size()};

    // Write each peer
    for (const auto& [peerAddr, peerStatus] : peers)
    {
        // Address length and string
        peersFileBytes << static_cast<uint16_t>(peerAddr.address.size());
        peersFileBytes << peerAddr.address;

        // Port
        peersFileBytes << peerAddr.port;

        // Services
        peersFileBytes << peerStatus.services;

        // Last seen
        peersFileBytes << peerStatus.lastSeen;
    }

    peersFile.write(peersFileBytes.cdata(), peersFileBytes.ssize());
}

std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> loadPeers()
{
    if (!fs::exists(paths::peers))
    {
        return {};
    }

    std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> peers;
    auto peersFileBytes = readWholeFile(paths::peers);

    // Read peer count
    uint64_t peersCount;
    peersFileBytes >> peersCount;

    // Read each peer
    for (uint64_t i = 0; i < peersCount; i++)
    {
        PeerAddress peerAddr;
        PeerStatus peerStatus;

        // Read address length
        uint16_t addrLen;
        peersFileBytes >> addrLen;

        // Read address string
        peersFileBytes >> peerAddr.address;

        // Read port
        peersFileBytes >> peerAddr.port;

        // Read services
        peersFileBytes >> peerStatus.services;

        // Read last seen
        peersFileBytes >> peerStatus.lastSeen;

        peers.insert({peerAddr, peerStatus});
    }

    return peers;
}
