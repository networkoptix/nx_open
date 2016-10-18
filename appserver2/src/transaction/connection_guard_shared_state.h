#pragma once

#include <QMap>
#include <QSet>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

namespace ec2 {

class ConnectionGuardSharedState
{
public:
    /**
     * first - true if connecting to remove peer in progress,
     * second - true if getting connection from remove peer in progress
     */
    typedef QMap<QnUuid, QPair<bool, bool>> ConnectingInfoMap;

    ConnectingInfoMap m_connectingList;
    QSet<QnUuid> m_connectedList;
    QnMutex m_mutex;
};

} // namespace ec2
