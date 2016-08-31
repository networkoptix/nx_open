#pragma once

#include <nx/utils/uuid.h>
#include <nx/utils/thread/mutex.h>
#include <QtCore/QMap>
#include <QtCore/QSet>

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

        ConnectionLockGuard();
        ConnectionLockGuard(const QnUuid& peerGuid, Direction direction);
        ConnectionLockGuard(ConnectionLockGuard&&);
        ConnectionLockGuard& operator=(ConnectionLockGuard&&);
        ~ConnectionLockGuard();

        bool tryAcquireConnecting();
        bool tryAcquireConnected();
        bool isNull() const;
    private:
        QnUuid m_peerGuid;
        Direction m_direction;
        State m_state;

        ConnectionLockGuard(const ConnectionLockGuard&);
        ConnectionLockGuard& operator=(const ConnectionLockGuard&);

        void removeFromConnectingListNoLock();

        /** first - true if connecting to remove peer in progress,
         * second - true if getting connection from remove peer in progress
         */
        typedef QMap<QnUuid, QPair<bool, bool>> ConnectingInfoMap;

        static ConnectingInfoMap m_connectingList;
        static QSet<QnUuid> m_connectedList;
        static QnMutex m_staticMutex;
    };

} // namespace ec2
