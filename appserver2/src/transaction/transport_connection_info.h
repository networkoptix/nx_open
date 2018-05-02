#pragma once

#include <QtCore/QUrl>

#include <nx/utils/uuid.h>
#include <nx/fusion/fusion/fusion_fwd.h>
#include <nx_ec/data/api_peer_data.h>

namespace ec2
{
    struct QnTransportConnectionInfo
    {
        QnUuid remotePeerId;
        nx::utils::Url url;
        QString state;
        bool isIncoming = false;
        bool isStarted = false;
        QVector<ApiPersistentIdData> subscription;
    };

#define QnTransportConnectionInfo_Fields (remotePeerId)(url)(state)(isIncoming)(isStarted)(subscription)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnTransportConnectionInfo), (json));

} // namespace ec2
