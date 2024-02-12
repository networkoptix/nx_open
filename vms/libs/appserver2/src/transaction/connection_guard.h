// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QMap>
#include <QtCore/QSet>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/uuid.h>

#include "connection_guard_shared_state.h"

namespace ec2
{

    /** While this object is alive peer with guid \a peerGuid is considered
        connected and cannot establish another transaction connection.
        Calls \a QnTransactionTransport::connectDone(m_peerGuid) in destructor
    */
    class ConnectionLockGuard
    {
    public:

        enum class Direction
        {
            Outgoing,
            Incoming
        };

        enum class State
        {
            Initial,
            Connecting,
            Connected
        };

        ConnectionLockGuard(
            const nx::Uuid& localId,
            ConnectionGuardSharedState* const sharedState);
        ConnectionLockGuard(
            const nx::Uuid& localId,
            ConnectionGuardSharedState* const sharedState,
            const nx::Uuid& peerGuid,
            Direction direction);
        ConnectionLockGuard(ConnectionLockGuard&&);
        ConnectionLockGuard& operator=(ConnectionLockGuard&&);
        ~ConnectionLockGuard();

        bool tryAcquireConnecting();
        bool tryAcquireConnected();
        bool isNull() const;

    private:
        nx::Uuid m_localId;
        ConnectionGuardSharedState* const m_sharedState;
        nx::Uuid m_peerGuid;
        Direction m_direction;
        State m_state;

        ConnectionLockGuard(const ConnectionLockGuard&) = delete;
        ConnectionLockGuard& operator=(const ConnectionLockGuard&) = delete;

        void removeFromConnectingListNoLock();
    };

} // namespace ec2
