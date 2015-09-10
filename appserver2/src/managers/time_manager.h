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
#include <utils/common/enable_multi_thread_direct_connection.h>
#include <utils/common/id.h>
#include <utils/common/timermanager.h>
#include <utils/common/singleton.h>
#include <utils/network/time/abstract_accurate_time_fetcher.h>
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
    /*!
        \note \a sequence has less priority than \a TimeSynchronizationManager::peerIsServer and \a TimeSynchronizationManager::peerTimeSynchronizedWithInternetServer flags
    */
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
        bool operator!=( const TimePriorityKey& right ) const;
        bool operator<( const TimePriorityKey& right ) const;
        bool operator<=( const TimePriorityKey& right ) const;
        bool operator>( const TimePriorityKey& right ) const;
        quint64 toUInt64() const;
        void fromUInt64( quint64 val );
    };

    class TimeSyncInfo
    {
    public:
        qint64 monotonicClockValue;
        //!synchronized millis from epoch, corresponding to \a monotonicClockValue
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
        public QObject,
        public QnStoppable,
        public EnableMultiThreadDirectConnection<TimeSynchronizationManager>,
        public Singleton<TimeSynchronizationManager>
    {
        Q_OBJECT

    public:
        //!Need this flag to synchronize by server peer only
        static const int peerIsServer                            = 0x1000;
        static const int peerTimeSynchronizedWithInternetServer  = 0x0008;
        static const int peerTimeSetByUser                       = 0x0004;
        static const int peerHasMonotonicClock                   = 0x0002;
        static const int peerIsNotEdgeServer                     = 0x0001;

        /*!
            \note \a TimeSynchronizationManager::start MUST be called before using class instance
        */
        TimeSynchronizationManager( Qn::PeerType peerType );
        virtual ~TimeSynchronizationManager();

        //!Implemenattion of QnStoppable::pleaseStop
        virtual void pleaseStop() override;

        //!Initial initialization
        /*!
            \note Cannot do it in constructor to keep valid object destruction order
            TODO #ak look like incapsulation failure. Better remove this method
        */
        void start();
        
        //!Returns synchronized time (millis from epoch, UTC)
        qint64 getSyncTime() const;
        //!Called when primary time server has been changed by user
        void primaryTimeServerChanged( const QnTransaction<ApiIdData>& tran );
        void peerSystemTimeReceived( const QnTransaction<ApiPeerSystemTimeData>& tran );
        void knownPeersSystemTimeReceived( const QnTransaction<ApiPeerSystemTimeDataList>& tran );
        //!Returns synchronized time with time priority key (not local, but the one used)
        TimeSyncInfo getTimeSyncInfo() const;
        //!Returns value of internal monotonic clock
        qint64 getMonotonicClock() const;
        //!Resets synchronized time to local system time with local peer priority
        void forgetSynchronizedTime();
        //!Reset sync time and resynce
        void forceTimeResync();
        QnPeerTimeInfoList getPeerTimeInfoList() const;
        ApiPeerSystemTimeDataList getKnownPeersSystemTime() const;
        void processTimeSyncInfoHeader(
            const QnUuid& peerID,
            const nx_http::StringType& serializedTimeSync,
            AbstractStreamSocket* sock );

    signals:
        //!Emitted when there is ambiguity while choosing primary time server automatically
        /*!
            User SHOULD call \a TimeSynchronizationManager::forcePrimaryTimeServer to set primary time server manually.
            This signal is emitted periodically until ambiguity in choosing primary time server has been resolved (by user or automatically)
        */
        void primaryTimeServerSelectionRequired();
        //!Emitted when synchronized time has been changed
        void timeChanged( qint64 syncTime );
        //!Emitted when peer \a peerId local time has changed
        /*!
            \param peerId
            \param syncTime Synchronized time (UTC, millis from epoch) corresponding to \a peerLocalTime
            \param peerLocalTime Peer local time (UTC, millis from epoch)
        */
        void peerTimeChanged(const QnUuid &peerId, qint64 syncTime, qint64 peerLocalTime);

    private:
        struct RemotePeerTimeInfo
        {
            QnUuid peerID;
            qint64 localMonotonicClock;
            //!synchronized millis from epoch, corresponding to \a monotonicClockValue
            qint64 remotePeerSyncTime;

            RemotePeerTimeInfo(
                const QnUuid& _peerID = QnUuid(),
                qint64 _localMonotonicClock = 0,
                qint64 _remotePeerSyncTime = 0 )
            :
                peerID( _peerID ),
                localMonotonicClock( _localMonotonicClock ),
                remotePeerSyncTime( _remotePeerSyncTime )
            {
            }
        };

        struct PeerContext
        {
            SocketAddress peerAddress;
            nx_http::AuthInfoCache::AuthorizationCacheItem authData;
            TimerManager::TimerGuard syncTimerID;
            nx_http::AsyncHttpClientPtr httpClient;

            PeerContext(
                SocketAddress _peerAddress,
                nx_http::AuthInfoCache::AuthorizationCacheItem _authData )
            :
                peerAddress( std::move( _peerAddress ) ),
                authData( std::move(_authData) )
            {}

            //PeerContext( PeerContext&& right ) = default;
            PeerContext( PeerContext&& right )
            :
                peerAddress( std::move(right.peerAddress) ),
                authData( std::move(right.authData) ),
                syncTimerID( std::move(right.syncTimerID) ),
                httpClient( std::move(right.httpClient) )
            {}

            //PeerContext& operator=( PeerContext&& right ) = default;
            PeerContext& operator=( PeerContext&& right )
            {
                peerAddress = std::move(right.peerAddress);
                syncTimerID = std::move(right.syncTimerID);
                httpClient = std::move(right.httpClient);
                return *this;
            }
        };

        //!Delta (millis) from \a m_monotonicClock to local time, synchronized with internet
        qint64 m_localSystemTimeDelta;
        //!Using monotonic clock to be proof to local system time change
        QElapsedTimer m_monotonicClock;
        //!priority key of current server
        TimePriorityKey m_localTimePriorityKey;
        mutable QMutex m_mutex;
        TimeSyncInfo m_usedTimeSyncInfo;
        quint64 m_broadcastSysTimeTaskID;
        quint64 m_internetSynchronizationTaskID;
        quint64 m_manualTimerServerSelectionCheckTaskID;
        bool m_terminated;
        /*!
            \a TimeSyncInfo::syncTime stores local time on specified server
        */
        std::map<QnUuid, TimeSyncInfo> m_systemTimeByPeer;
        const Qn::PeerType m_peerType;
        std::unique_ptr<AbstractAccurateTimeFetcher> m_timeSynchronizer;
        size_t m_internetTimeSynchronizationPeriod;
        bool m_timeSynchronized;
        int m_internetSynchronizationFailureCount;
        std::map<QnUuid, PeerContext> m_peersToSendTimeSyncTo;

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
            const QnUuid& remotePeerID,
            qint64 localMonotonicClock,
            qint64 remotePeerSyncTime,
            const TimePriorityKey& remotePeerTimePriorityKey,
            qint64 timeErrorEstimation );
        void broadcastLocalSystemTime( quint64 taskID );
        void checkIfManualTimeServerSelectionIsRequired( quint64 taskID );
        //!Periodically synchronizing time with internet (if possible)
        void syncTimeWithInternet( quint64 taskID );
        void onTimeFetchingDone( qint64 millisFromEpoch, SystemError::ErrorCode errorCode );
        void addInternetTimeSynchronizationTask();
        //!Returns time received from the internet or current local system time
        qint64 currentMSecsSinceEpoch() const;

        void updateRuntimeInfoPriority(quint64 priority);
        void peerSystemTimeReceivedNonSafe( const ApiPeerSystemTimeData& tran );
        qint64 getSyncTimeNonSafe() const;
        void startSynchronizingTimeWithPeer(
            const QnUuid& peerID,
            SocketAddress peerAddress,
            nx_http::AuthInfoCache::AuthorizationCacheItem authData );
        void stopSynchronizingTimeWithPeer( const QnUuid& peerID );
        void synchronizeWithPeer( const QnUuid& peerID );
        void timeSyncRequestDone(
            const QnUuid& peerID,
            nx_http::AsyncHttpClientPtr clientPtr );
        TimeSyncInfo getTimeSyncInfoNonSafe() const;
        void syncTimeWithAllKnownServers(QMutexLocker* const lock);
        void onBeforeSendingTransaction(
            QnTransactionTransport* transport,
            nx_http::HttpHeaders* const headers);
        void onTransactionReceived(
            QnTransactionTransport* transport,
            const nx_http::HttpHeaders& headers);
        void forgetSynchronizedTimeNonSafe(QMutexLocker* const /*lock*/);

    private slots:
        void onNewConnectionEstablished(QnTransactionTransport* transport );
        void onPeerLost( ApiPeerAliveData data );
        void onDbManagerInitialized();
    };
}

#endif  //NX_EC2_TIME_MANAGER_H
