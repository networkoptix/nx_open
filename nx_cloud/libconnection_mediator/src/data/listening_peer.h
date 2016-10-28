#pragma once

#include <map>
#include <vector>

#include <QtCore/QString>

#include <nx/network/buffer.h>

#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace hpm {
namespace data {

struct ListeningPeer
{
    QString connectionEndpoint;
    std::vector<QString> directTcpEndpoints;
};

#define ListeningPeer_Fields (connectionEndpoint)(directTcpEndpoints)
typedef std::map<QString /*peerId*/, ListeningPeer> ListeningPeersById;
typedef std::map<QString /*systemId*/, ListeningPeersById> ListeningPeersBySystems;

struct BoundClientInfo
{
    QString connectionEndpoint;
    std::vector<QString> tcpReverseEndpoints;
};

#define BoundClientInfo_Fields (connectionEndpoint)(tcpReverseEndpoints)
typedef std::map<QString, BoundClientInfo> BoundClientInfos;

struct ListeningPeers
{
    ListeningPeersBySystems systems;
    BoundClientInfos clients;
};

#define ListeningPeers_Fields (systems)(clients)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES(
    (ListeningPeer)(BoundClientInfo)(ListeningPeers),
    (json));

} // namespace data
} // namespace hpm
} // namespace nx
