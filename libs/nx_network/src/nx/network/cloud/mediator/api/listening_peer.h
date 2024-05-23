// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <map>
#include <string>
#include <vector>

#include <nx/reflect/instrument.h>
#include <nx/utils/buffer.h>

#include "connection_speed.h"

namespace nx::hpm::api {

struct ListeningPeer
{
    /**%apidoc Auto-detected endpoint of the peer. */
    std::string connectionEndpoint;

    /**%apidoc Endpoints reported by the peer. */
    std::vector<std::string> directTcpEndpoints;

    /**%apidoc Peer's uplink statistics as reported by the peer. */
    std::optional<nx::hpm::api::ConnectionSpeed> uplinkSpeed;
};

#define ListeningPeer_Fields (connectionEndpoint)(directTcpEndpoints)(uplinkSpeed)

NX_REFLECTION_INSTRUMENT(ListeningPeer, ListeningPeer_Fields)

/**%apidoc Dictionary of id to listening peer descriptor. */
using ListeningPeersById = std::map<std::string /*peerId*/, ListeningPeer>;

using ListeningPeersBySystem = std::map<std::string /*systemId*/, ListeningPeersById>;

struct BoundClient
{
    std::string connectionEndpoint;
    std::vector<std::string> tcpReverseEndpoints;
};

#define BoundClient_Fields (connectionEndpoint)(tcpReverseEndpoints)
typedef std::map<std::string /*clientId*/, BoundClient> BoundClientById;

NX_REFLECTION_INSTRUMENT(BoundClient, BoundClient_Fields)

struct SystemPeers
{
    /**%apidoc[readonly] Peers. */
    ListeningPeersById peers;

    /**%apidoc[readonly] Id of the server that is selected when client requests connection to a system. */
    std::optional<std::string> preferredPeerId;
};

#define SystemPeers_Fields (peers)(preferredPeerId)

NX_REFLECTION_INSTRUMENT(SystemPeers, SystemPeers_Fields)

struct ListeningPeers
{
    ListeningPeersBySystem systems;
    BoundClientById clients;
};

#define ListeningPeers_Fields (systems)(clients)

NX_REFLECTION_INSTRUMENT(ListeningPeers, ListeningPeers_Fields)

} // namespace nx::hpm::api
