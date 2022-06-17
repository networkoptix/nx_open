// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/peer_data.h>
#include <nx/vms/api/data/p2p_statistics_data.h>

namespace ec2
{
struct QnTransportConnectionInfo
{
    QnUuid remotePeerId;
    QnUuid remotePeerDbId;
    /**%apidoc:string */
    nx::utils::Url url;
    QString state;
    QString previousState;
    bool isIncoming = false;
    bool isStarted = false;
    bool gotPeerInfo = false;
    nx::vms::api::PeerType peerType;
    std::vector<nx::vms::api::PersistentIdData> subscribedTo;
    std::vector<nx::vms::api::PersistentIdData> subscribedFrom;
};

struct ConnectionInfos
{
    std::vector<QnTransportConnectionInfo> connections;
    nx::vms::api::PersistentIdData idData;
};

#define QnTransportConnectionInfo_Fields \
    (remotePeerId)(remotePeerDbId)(url)(state)(previousState)(isIncoming)(isStarted)(gotPeerInfo)(peerType) \
    (subscribedTo)(subscribedFrom)

#define ConnectionInfos_Fields \
    (connections)(idData)

QN_FUSION_DECLARE_FUNCTIONS(QnTransportConnectionInfo, (json))
QN_FUSION_DECLARE_FUNCTIONS(ConnectionInfos, (json))

struct P2pStats
{
    QnUuid serverId;
    nx::vms::api::P2pStatisticsData data;
    ConnectionInfos connections;
};
#define P2pStats_Fields (serverId)(data)(connections)
NX_VMS_API_DECLARE_STRUCT(P2pStats)

} // namespace ec2
