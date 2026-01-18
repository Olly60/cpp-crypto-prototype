#pragma once
#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <asio/ip/address.hpp>
#include <asio/ip/tcp.hpp>
#include "crypto_utils.h"

struct AddressHash
{
    std::size_t operator()(const asio::ip::address& addr) const noexcept
    {
        std::size_t h = 0;

        if (addr.is_v4())
        {
            auto bytes = addr.to_v4().to_bytes();
            for (auto b : bytes)
                h = (h * 131) ^ b;
        }
        else
        {
            auto bytes = addr.to_v6().to_bytes();
            for (auto b : bytes)
                h = (h * 131) ^ b;
        }

        return h;
    }
};

struct EndpointHash
{
    std::size_t operator()(const asio::ip::tcp::endpoint& ep) const noexcept
    {
        const auto& addr = ep.address();

        std::size_t h = 0;

        if (addr.is_v4())
        {
            auto bytes = addr.to_v4().to_bytes();
            for (auto b : bytes)
                h = (h * 131) ^ b;
        }
        else
        {
            auto bytes = addr.to_v6().to_bytes();
            for (auto b : bytes)
                h = (h * 131) ^ b;
        }

        // mix in port
        h ^= std::hash<unsigned short>{}(ep.port()) + 0x9e3779b9 + (h << 6) + (h >> 2);

        return h;
    }
};

const std::filesystem::path KNOWN_PEERS = "known_peers.dat";
const std::filesystem::path UNKNOWN_PEERS = "unknown_peers.dat";

struct PeerStatus
{
    uint64_t services{};
    uint64_t lastSeen{};
    uint8_t relay{};
    Array256_t tip{};
    uint16_t port{};
};

using KnownPeersMap = std::unordered_map<asio::ip::address, PeerStatus, AddressHash>;

using UnknownPeersMap = std::unordered_set<asio::ip::tcp::endpoint, EndpointHash>;

// Global state
inline KnownPeersMap knownPeers;
inline UnknownPeersMap unknownPeers;

void storePeers();

void loadPeers();

asio::ip::address normalizeAddress(asio::ip::address addr);