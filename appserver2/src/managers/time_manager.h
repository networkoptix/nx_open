#pragma once

#include <map>

#include <nx/utils/thread/mutex.h>
#include <QtCore/QObject>
#include <QtGlobal>
#include <QtCore/QElapsedTimer>

#include <common/common_globals.h>
#include <nx_ec/ec_api.h>
#include <nx_ec/data/api_peer_system_time_data.h>
#include <utils/common/enable_multi_thread_direct_connection.h>
#include <utils/common/id.h>
#include <nx/utils/thread/stoppable.h>
#include <nx/utils/timer_manager.h>
#include <nx/utils/safe_direct_connection.h>
#include <nx/utils/singleton.h>
#include <nx/network/time/abstract_accurate_time_fetcher.h>
#include <nx/network/http/http_types.h>

#include "nx_ec/data/api_data.h"
#include "transaction/transaction.h"
#include "transaction/transaction_transport.h"

#include <nx/utils/thread/mutex.h>

/**
 * Time synchronization in cluster.
 * Server system time is never changed. To adjust server times means, adjust server "delta" which server adds to it's time.
 * To change/adjust server time - means to adjust it's delta.
 *
 * PTS(primary time server) (if any) pushes it's time to other servers( adjusts other servers' time):\n
 * - every hour (to all servers in the system)
 * - upon PTS changed event (to all servers in the system)
 * - if new server is connected to the system( only to this new server).
 *
 * If possible PTS server adjusts it's time once per hour from internet.
 *
 * PTS is selected in prioritized order: \n
 * - selected by user
 * - availability of internet access
 * - non edge server
 *
 * In case if there is ambiguity in choosing PTS automatically, random server is chosen as PTS and notification sent to client.
 *
 * Once PTS pushed it's time to the server, server marks it's time as actual time. Time remains actual till server is restarted.
 * If there is no PTS in the system, new server( just connected) takes time from any server with actual time.
 *
 * Client should offer user to select PTS if there is no PTS in the system and there are at least 2 servers in the system with actual time more than 500 ms different.
 *
 * \todo if system without internet access (with PTS selected) joined by a server with internet access, should new server become PTS?
 */

namespace ec2 {

class Ec2DirectConnection;
class Settings;
class AbstractTransactionMessageBus;

/**
 * Sequence has less priority than TimeSynchronizationManager::peerIsServer and
 * TimeSynchronizationManager::peerTimeSynchronizedWithInternetServer flags
 */
class TimePriorityKey
{
public:
        //!sequence number. Incremented with each peer selection by user
    quint16 sequence;
        //!bitset of flags from \a TimeSynchronizationManager class
    quint16 flags;
        //!some random number
    quint32 seed;

    TimePriorityKey();

    bool operator==(const TimePriorityKey& right) const;
    bool operator!=(const TimePriorityKey& right) const;

    bool hasLessPriorityThan(
        const TimePriorityKey& right,
        bool takeIntoAccountInternetTime) const;
    quint64 toUInt64() const;
    void fromUInt64(quint64 val);

    bool isTakenFromInternet() const;
};

class TimeSyncInfo
{
public:
    qint64 monotonicClockValue;
    /** Synchronized millis from epoch, corresponding to monotonicClockValue. */
    qint64 syncTime;
    /**
     * Priority key, corresponding to syncTime value. This is not necessarily priority key of current server.
     * When local host accepts another peer's time it uses priority key of that peer.
     */
    TimePriorityKey timePriorityKey;

    TimeSyncInfo(
        qint64 _monotonicClockValue = 0,
        qint64 _syncTime = 0,
        const TimePriorityKey& _timePriorityKey = TimePriorityKey() );

    QByteArray toString() const;
    bool fromString( const QByteArray& str );
};

/**
 * Cares about synchronizing time across all peers in cluster.
 * Time with highest priority key is selected as current.
 */
class TimeSynchronizationManager:
    public QObject,
    public QnStoppable,
    public EnableMultiThreadDirectConnection<TimeSynchronizationManager>,
    public Qn::EnableSafeDirectConnection
{
    Q_OBJECT

public:
    /**
     * TimeSynchronizationManager::start MUST be called before using class instance.
     */
    TimeSynchronizationManager(
        Qn::PeerType peerType,
        nx::utils::TimerManager* const timerManager,
        AbstractTransactionMessageBus* messageBus,
        Settings* settings);
    virtual ~TimeSynchronizationManager();

    /** Implemenattion of QnStoppable::pleaseStop. */
    virtual void pleaseStop() override;

    /**
     * Initial initialization.
     * @note Cannot do it in constructor to keep valid object destruction order.
     * TODO #ak look like incapsulation failure. Better remove this method.
     */
    void start(const std::shared_ptr<Ec2DirectConnection>& connection);

    /** Returns synchronized time (millis from epoch, UTC). */
    qint64 getSyncTime() const;
    ApiTimeData getTimeInfo() const;
    /** Called when primary time server has been changed by user. */
    void onGotPrimariTimeServerTran(const QnTransaction<ApiIdData>& tran);
    void primaryTimeServerChanged(const ApiIdData& serverId);
    /** Returns synchronized time with time priority key (not local, but the one used). */
    TimeSyncInfo getTimeSyncInfo() const;
    /** Returns value of internal monotonic clock. */
    qint64 getMonotonicClock() const;
    /** Resets synchronized time to local system time with local peer priority. */
    void forgetSynchronizedTime();
    /** Reset sync time and resynce. */
    void forceTimeResync();
    void processTimeSyncInfoHeader(
        const QnUuid& peerID,
        const nx::network::http::StringType& serializedTimeSync,
        boost::optional<qint64> requestRttMillis);
    void resyncTimeWithPeer(const QnUuid& peerId);

signals:
    /** Emitted when synchronized time has been changed. */
    void timeChanged( qint64 syncTime );

private:
    struct RemotePeerTimeInfo
    {
        QnUuid peerID;
        qint64 localMonotonicClock;
        /** Synchronized millis from epoch, corresponding to monotonicClockValue. */
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
        nx::network::SocketAddress peerAddress;
        nx::network::http::AuthInfoCache::AuthorizationCacheItem authData;
        nx::utils::TimerManager::TimerGuard syncTimerID;
        nx::network::http::AsyncHttpClientPtr httpClient;
        /** Request round-trip time. */
        boost::optional<qint64> rttMillis;

        PeerContext(
            nx::network::SocketAddress _peerAddress,
            nx::network::http::AuthInfoCache::AuthorizationCacheItem _authData )
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

    /** Delta (millis) from m_monotonicClock to local time, synchronized with internet. */
    qint64 m_localSystemTimeDelta;
    /** Using monotonic clock to be proof to local system time change. */
    QElapsedTimer m_monotonicClock;
    //!priority key of current server
    TimePriorityKey m_localTimePriorityKey;
    mutable QnMutex m_mutex;
    TimeSyncInfo m_usedTimeSyncInfo;
    quint64 m_internetSynchronizationTaskID;
    quint64 m_manualTimerServerSelectionCheckTaskID;
    quint64 m_checkSystemTimeTaskID;
    boost::optional<qint64> m_prevSysTime;
    boost::optional<qint64> m_prevMonotonicClock;
    bool m_terminated;
    std::shared_ptr<Ec2DirectConnection> m_connection;
    AbstractTransactionMessageBus* m_messageBus;
    const Qn::PeerType m_peerType;
    nx::utils::TimerManager* const m_timerManager;
    std::unique_ptr<AbstractAccurateTimeFetcher> m_timeSynchronizer;
    size_t m_internetTimeSynchronizationPeriod;
    bool m_timeSynchronized;
    int m_internetSynchronizationFailureCount;
    std::map<QnUuid, PeerContext> m_peersToSendTimeSyncTo;
    Settings* m_settings;
    std::atomic<size_t> m_asyncOperationsInProgress;
    QnWaitCondition m_asyncOperationsWaitCondition;

    void selectLocalTimeAsSynchronized(
        QnMutexLockerBase* const lock,
        quint16 newTimePriorityKeySequence);
    /**
     * @param lock Locked m_mutex. This method will unlock it to emit TimeSynchronizationManager::timeChanged signal.
     * @param remotePeerID
     * @param localMonotonicClock value of local monotonic clock (received with TimeSynchronizationManager::monotonicClockValue).
     * @param remotePeerSyncTime remote peer time (millis, UTC from epoch) corresponding to local clock (localClock).
     * @param remotePeerTimePriorityKey This value is used to select peer to synchronize time with.
     *   - upper DWORD is bitset of flags peerTimeSetByUser, peerTimeSynchronizedWithInternetServer and peerTimeNonEdgeServer.
     *   - low DWORD - some random number
     */
    void remotePeerTimeSyncUpdate(
        QnMutexLockerBase* const lock,
        const QnUuid& remotePeerID,
        qint64 localMonotonicClock,
        qint64 remotePeerSyncTime,
        const TimePriorityKey& remotePeerTimePriorityKey,
        qint64 timeErrorEstimation );
    void checkIfManualTimeServerSelectionIsRequired( quint64 taskID );
    /** Periodically synchronizing time with internet (if possible). */
    void syncTimeWithInternet( quint64 taskID );
    void onTimeFetchingDone( qint64 millisFromEpoch, SystemError::ErrorCode errorCode );
    void initializeTimeFetcher();
    void addInternetTimeSynchronizationTask();
    /** Returns time received from the internet or current local system time. */
    qint64 currentMSecsSinceEpoch() const;

    void updateRuntimeInfoPriority(quint64 priority);
    qint64 getSyncTimeNonSafe() const;
    void startSynchronizingTimeWithPeer(
        const QnUuid& peerID,
        nx::network::SocketAddress peerAddress,
        nx::network::http::AuthInfoCache::AuthorizationCacheItem authData );
    void stopSynchronizingTimeWithPeer( const QnUuid& peerID );
    void synchronizeWithPeer( const QnUuid& peerID );
    void timeSyncRequestDone(
        const QnUuid& peerID,
        nx::network::http::AsyncHttpClientPtr clientPtr,
        qint64 requestRttMillis);
    TimeSyncInfo getTimeSyncInfoNonSafe() const;
    void syncTimeWithAllKnownServers(const QnMutexLockerBase& /*lock*/);
    void onBeforeSendingTransaction(
        QnTransactionTransportBase* transport,
        nx::network::http::HttpHeaders* const headers);
    void onTransactionReceived(
        QnTransactionTransportBase* transport,
        const nx::network::http::HttpHeaders& headers);
    void forgetSynchronizedTimeNonSafe(QnMutexLockerBase* const lock);
    void switchBackToLocalTime(QnMutexLockerBase* const /*lock*/);
    void checkSystemTimeForChange();
    void handleLocalTimePriorityKeyChange(QnMutexLockerBase* const lock);

    void saveSyncTimeAsync(
        QnMutexLockerBase* const lock,
        qint64 syncTimeToLocalDelta,
        TimePriorityKey syncTimeKey);

    void saveSyncTimeAsync(
        qint64 syncTimeToLocalDelta,
        const TimePriorityKey& syncTimeKey);
    bool saveSyncTimeSync(
        qint64 syncTimeToLocalDelta,
        const TimePriorityKey& syncTimeKey);

private slots:
    void onNewConnectionEstablished(QnAbstractTransactionTransport* transport );
    void onPeerLost(QnUuid peer, Qn::PeerType peerType);
    void onDbManagerInitialized();
    void onTimeSynchronizationSettingsChanged();
};

} // namespace ec2
