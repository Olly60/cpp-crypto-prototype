#include "storage/peers.h"
#include "storage/storage_utils.h"

void storePeers(const std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash>& peers)
{

    std::ofstream peersFile(PEERS, std::ios::trunc | std::ios::binary);
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

    writeFile(peersFile, peersFileBytes);
}

std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> loadPeers()
{
    if (!std::filesystem::exists(PEERS))
    {
        return {};
    }

    std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> peers;
    auto peersFileBytes = readFile(PEERS);
    if (!peersFileBytes) return peers;

    // Read peer count
    uint64_t peersCount = peersFileBytes->readU16();

    // Read each peer
    for (uint64_t i = 0; i < peersCount; i++)
    {
        PeerAddress peerAddr;
        PeerStatus peerStatus;

        // Read address string
        peerAddr.address = peersFileBytes->readString();

        // Read port
        peerAddr.port = peersFileBytes->readU16();

        // Read services
        peerStatus.services = peersFileBytes->readU64();

        // Read last seen
        peerStatus.lastSeen = peersFileBytes->readU64();

        peers.insert({peerAddr, peerStatus});
    }

    return peers;
}
