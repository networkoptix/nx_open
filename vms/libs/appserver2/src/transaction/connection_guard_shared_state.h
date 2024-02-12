// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

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
    typedef QMap<nx::Uuid, QPair<bool, bool>> ConnectingInfoMap;

    bool contains(const nx::Uuid& id) const;

    ConnectingInfoMap m_connectingList;
    QSet<nx::Uuid> m_connectedList;
    mutable nx::Mutex m_mutex;
};

} // namespace ec2
