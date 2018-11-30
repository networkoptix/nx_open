#pragma once

#include <QtCore/QUrl>

#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx/utils/url.h>
#include <nx/utils/uuid.h>
#include <nx/vms/api/data/peer_data.h>

namespace ec2
{
    struct QnTransportConnectionInfo
    {
        QnUuid remotePeerId;
        nx::utils::Url url;
        QString state;
        QString previousState;
        bool isIncoming = false;
        bool isStarted = false;
        bool gotPeerInfo = false;
        QVector<nx::vms::api::PersistentIdData> subscription;
        nx::vms::api::PeerType peerType;
    };

#define QnTransportConnectionInfo_Fields \
    (remotePeerId)(url)(state)(previousState)(isIncoming)(isStarted)(gotPeerInfo)(subscription)(peerType)

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnTransportConnectionInfo), (json));

} // namespace ec2
