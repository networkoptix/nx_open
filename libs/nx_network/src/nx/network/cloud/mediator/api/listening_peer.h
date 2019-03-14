#pragma once

#include <map>
#include <vector>

#include <QtCore/QString>

#include <nx/network/buffer.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace hpm {
namespace api {

struct ListeningPeer
{
    QString connectionEndpoint;
    std::vector<QString> directTcpEndpoints;
};

#define ListeningPeer_Fields (connectionEndpoint)(directTcpEndpoints)
typedef std::map<QString /*peerId*/, ListeningPeer> ListeningPeersById;
typedef std::map<QString /*systemId*/, ListeningPeersById> ListeningPeersBySystem;

struct BoundClient
{
    QString connectionEndpoint;
    std::vector<QString> tcpReverseEndpoints;
};

#define BoundClient_Fields (connectionEndpoint)(tcpReverseEndpoints)
typedef std::map<QString /*clientId*/, BoundClient> BoundClientById;

struct ListeningPeers
{
    ListeningPeersBySystem systems;
    BoundClientById clients;
};

#define ListeningPeers_Fields (systems)(clients)

QN_FUSION_DECLARE_FUNCTIONS(nx::hpm::api::ListeningPeer, (json), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::hpm::api::BoundClient, (json), NX_NETWORK_API)
QN_FUSION_DECLARE_FUNCTIONS(nx::hpm::api::ListeningPeers, (json), NX_NETWORK_API)

} // namespace api {
} // namespace hpm
} // namespace nx
