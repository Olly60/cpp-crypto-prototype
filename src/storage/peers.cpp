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

    // Write peer count
    appendToFile(peersFile, peers.size());

    // Write each peer
    for (const auto& [peerAddr, peerStatus] : peers)
    {
        // Address length and string
        appendToFile(peersFile, static_cast<uint16_t>(peerAddr.address.size()));
        peersFile.write(peerAddr.address.data(), peerAddr.address.size());

        // Port
        appendToFile(peersFile, peerAddr.port);

        // Services
        appendToFile(peersFile, peerStatus.services);

        // Last seen
        appendToFile(peersFile, peerStatus.lastSeen);
    }
}

std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> loadPeers()
{
    if (!fs::exists(paths::peers))
    {
        return {};
    }

    std::unordered_map<PeerAddress, PeerStatus, PeerAddressHash> peers;
    auto peersFileBytes = readWholeFile(paths::peers);
    size_t offset = 0;

    // Read peer count
    uint64_t peersCount;
    parseBytesInto(peersCount, peersFileBytes, offset);

    // Read each peer
    for (uint64_t i = 0; i < peersCount; i++)
    {
        PeerAddress peerAddr;
        PeerStatus peerStatus;

        // Read address length
        uint16_t addrLen;
        parseBytesInto(addrLen, peersFileBytes, offset);

        // Read address string
        if (offset + addrLen > peersFileBytes.size())
        {
            throw std::runtime_error("Peers file corrupted: address exceeds file size");
        }
        peerAddr.address = std::string(
            reinterpret_cast<const char*>(peersFileBytes.data() + offset),
            addrLen
        );
        offset += addrLen;

        // Read port
        parseBytesInto(peerAddr.port, peersFileBytes, offset);

        // Read services
        parseBytesInto(peerStatus.services, peersFileBytes, offset);

        // Read last seen
        parseBytesInto(peerStatus.lastSeen, peersFileBytes, offset);

        peers.insert({peerAddr, peerStatus});
    }

    return peers;
}
