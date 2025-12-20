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
    peersFileBytes.writeU64(peers.size());

    // Write each peer
    for (const auto& [peerAddr, peerStatus] : peers)
    {
        // Address length and string
        peersFileBytes.writeU64(peerAddr.address.size());
        peersFileBytes.writeString(peerAddr.address);

        // Port
        peersFileBytes.writeU16(peerAddr.port);

        // Services
        peersFileBytes.writeU64(peerStatus.services);

        // Last seen
        peersFileBytes.writeU64(peerStatus.lastSeen);
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
    uint64_t peersCount = peersFileBytes.readU64();

    // Read each peer
    for (uint64_t i = 0; i < peersCount; i++)
    {
        PeerAddress peerAddr;
        PeerStatus peerStatus;

        // Read address string
        peerAddr.address = peersFileBytes.readString();

        // Read port
        peerAddr.port = peersFileBytes.readU16();

        // Read services
        peerStatus.services = peersFileBytes.readU64();

        // Read last seen
        peerStatus.lastSeen = peersFileBytes.readU64();

        peers.insert({peerAddr, peerStatus});
    }

    return peers;
}
