/**********************************************************
* 02 jul 2014
* a.kolesnikov
***********************************************************/

#ifndef NX_EC2_TIME_MANAGER_H
#define NX_EC2_TIME_MANAGER_H

#include <map>

#include <QtCore/QMutex>
#include <QtCore/QObject>
#include <QtGlobal>
#include <QtCore/QElapsedTimer>

#include <common/common_globals.h>
#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_peer_system_time_data.h>
#include <utils/common/id.h>
#include <utils/network/daytime_nist_fetcher.h>
#include <utils/network/http/httptypes.h>

#include "nx_ec/data/api_data.h"
#include "transaction/transaction.h"
#include "transaction/transaction_transport.h"


/*! \page time_sync Time synchronization in cluster
    Server system time is never changed. To adjust server times means, adjust server "delta" which server adds to it's time. 
    To change/adjust server time - means to adjust it's delta.

    PTS(primary time server) (if any) pushes it's time to other servers( adjusts other servers' time):\n
    - every hour (to all servers in the system)
    - upon PTS changed event (to all servers in the system)
    - if new server is connected to the system( only to this new server).

    If possible PTS server adjusts it's time once per hour from internet.

    PTS is selected in prioritized order: \n
    - selected by user
    - availability of internet access
    - non edge server

    In case if there is ambiguity in choosing PTS automatically, random server is chosen as PTS and notification sent to client.

    Once PTS pushed it's time to the server, server marks it's time as actual time. Time remains actual till server is restarted. 
    If there is no PTS in the system, new server( just connected) takes time from any server with actual time.

    Client should offer user to select PTS if there is no PTS in the system and there are at least 2 servers in the system with actual time more than 500 ms different.

    \todo if system without internet access (with PTS selected) joined by a server with internet access, should new server become PTS?
*/

namespace ec2
{
    struct TimePriorityKey
    {
        //!sequence number. Incremented with each peer selection by user
        quint16 sequence;
        //!bitset of flags from \a TimeSynchronizationManager class
        quint16 flags;
        //!some random number
        quint32 seed;

        TimePriorityKey();

        bool operator==( const TimePriorityKey& right ) const;
        bool operator<( const TimePriorityKey& right ) const;
        bool operator>( const TimePriorityKey& right ) const;
        quint64 toUInt64() const;
        void fromUInt64( quint64 val );
    };

    class TimeSyncInfo
    {
    public:
        qint64 monotonicClockValue;
        //!synchorionized millis from epoch, corresponding to \a monotonicClockValue
        qint64 syncTime;
        //!priority key, corresponding to \a syncTime value. This is not necessarily priority key of current server
        /*!
            When local host accepts another peer's time it uses priority key of that peer
        */
        TimePriorityKey timePriorityKey;

        TimeSyncInfo(
            qint64 _monotonicClockValue = 0,
            qint64 _syncTime = 0,
            const TimePriorityKey& _timePriorityKey = TimePriorityKey() );

        QByteArray toString() const;
        bool fromString( const QByteArray& str );
    };

    //!Cares about synchronizing time across all peers in cluster
    /*!
        Time with highest priority key is selected as current
    */
    class TimeSynchronizationManager
    :
        public QObject
    {
        Q_OBJECT

    public:
        //!Need this flag to synchronize by server peer only
        static const int peerIsServer                            = 0x1000;
        static const int peerTimeSetByUser                       = 0x0008;
        static const int peerTimeSynchronizedWithInternetServer  = 0x0004;
        static const int peerHasMonotonicClock                   = 0x0002;
        static const int peerIsNotEdgeServer                     = 0x0001;

        TimeSynchronizationManager( Qn::PeerType peerType );
        virtual ~TimeSynchronizationManager();

        static TimeSynchronizationManager* instance();

        //!Returns synchronized time
        qint64 getSyncTime() const;
        //!Called when primary time server has been changed by user
        void primaryTimeServerChanged( const QnTransaction<ApiIdData>& tran );
        void peerSystemTimeReceived( const QnTransaction<ApiPeerSystemTimeData>& tran );
        //!Returns synchrionized time with time priority key (not local, but the one used)
        TimeSyncInfo getTimeSyncInfo() const;
        //!Returns value of internal monotonic clock
        qint64 getMonotonicClock() const;
        //!Priority key of local system. May differ from the one used (which returned by \a TimeSynchronizationManager::getTimeSyncInfo)
        TimePriorityKey localTimePriorityKey() const;

    signals:
        //!Emitted when there is ambiguity while choosing primary time server automatically
        /*!
            User SHOULD call \a TimeSynchronizationManager::forcePrimaryTimeServer to set primary time server manually.
            This signal is emitted periodically until ambiguity in choosing primary time server has been resolved (by user or automatically)
            \param localSystemTime Local system time (UTC, millis from epoch)
            \param peersAndTimes pair<peer id, peer local time (UTC, millis from epoch) corresponding to \a localSystemTime>
        */
        void primaryTimeServerSelectionRequired( qint64 localSystemTime, QList<QPair<QUuid, qint64> > peersAndTimes );
        //!Emitted when synchronized time has been changed
        void timeChanged( qint64 syncTime );

    private:
        struct RemotePeerTimeInfo
        {
            QUuid peerID;
            qint64 localMonotonicClock;
            //!synchorionized millis from epoch, corresponding to \a monotonicClockValue
            qint64 remotePeerSyncTime;

            RemotePeerTimeInfo(
                const QUuid& _peerID = QUuid(),
                qint64 _localMonotonicClock = 0,
                qint64 _remotePeerSyncTime = 0 )
            :
                peerID( _peerID ),
                localMonotonicClock( _localMonotonicClock ),
                remotePeerSyncTime( _remotePeerSyncTime )
            {
            }
        };

        //!Delta (millis) from \a m_monotonicClock to local time, synchronized with internet
        qint64 m_localSystemTimeDelta;
        //!Delta (millis) from \a m_monotonicClock to synchronized time. That is, sync_time = m_monotonicClock.elapsed + delta
        qint64 m_timeDelta;
        //!Using monotonic clock to be proof to local system time change
        QElapsedTimer m_monotonicClock;
        //!priority key of current server
        TimePriorityKey m_localTimePriorityKey;
        //!map<priority key, time info>
        std::map<quint64, RemotePeerTimeInfo, std::greater<quint64> > m_timeInfoByPeer;
        mutable QMutex m_mutex;
        TimeSyncInfo m_usedTimeSyncInfo;
        quint64 m_broadcastSysTimeTaskID;
        quint64 m_internetSynchronizationTaskID;
        quint64 m_manualTimerServerSelectionCheckTaskID;
        bool m_terminated;
        std::map<QUuid, TimeSyncInfo> m_systemTimeByPeer;
        const Qn::PeerType m_peerType;
        DaytimeNISTFetcher m_timeSynchronizer;
        size_t m_internetTimeSynchronizationPeriod;
        bool m_timeSynchronized;

        /*!
            \param lock Locked \a m_mutex. This method will unlock it to emit \a TimeSynchronizationManager::timeChanged signal
            \param remotePeerID
            \param localMonotonicClock value of local monotonic clock (received with \a TimeSynchronizationManager::monotonicClockValue)
            \param remotePeerSyncTime remote peer time (millis, UTC from epoch) corresponding to local clock (\a localClock)
            \param remotePeerTimePriorityKey This value is used to select peer to synchronize time with.\n
                - upper DWORD is bitset of flags \a peerTimeSetByUser, \a peerTimeSynchronizedWithInternetServer and \a peerTimeNonEdgeServer
                - low DWORD - some random number
        */
        void remotePeerTimeSyncUpdate(
            QMutexLocker* const lock,
            const QUuid& remotePeerID,
            qint64 localMonotonicClock,
            qint64 remotePeerSyncTime,
            const TimePriorityKey& remotePeerTimePriorityKey );
        void onBeforeSendingHttpChunk( QnTransactionTransport* transport, std::vector<nx_http::ChunkExtension>* const extensions );
        void onRecevingHttpChunkExtensions( QnTransactionTransport* transport, const std::vector<nx_http::ChunkExtension>& extensions );
        void broadcastLocalSystemTime( quint64 taskID );
        void checkIfManualTimeServerSelectionIsRequired( quint64 taskID );
        //!Periodically synchronizing time with internet (if possible)
        void syncTimeWithInternet( quint64 taskID );
        void onTimeFetchingDone( qint64 millisFromEpoch, SystemError::ErrorCode errorCode );
        //!Returns time received from the internet or current local system time
        qint64 currentMSecsSinceEpoch() const;

    private slots:
        void onNewConnectionEstablished( const QnTransactionTransportPtr& transport );
        void onPeerLost( ApiPeerAliveData data );
        void onDbManagerInitialized();
    };
}

#endif  //NX_EC2_TIME_MANAGER_H
