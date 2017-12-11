#include "time_manager.h"

#include <algorithm>
#include <atomic>

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QDateTime>
#include <nx/utils/thread/mutex.h>
#if defined(Q_OS_MACX) || defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(__aarch64__)
#include <zlib.h>
#else
#include <QtZlib/zlib.h>
#endif

#include <api/global_settings.h>
#include <api/runtime_info_manager.h>

#include <common/common_module.h>
#include <common/common_globals.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/network/http/custom_headers.h>
#include <transaction/abstract_transaction_message_bus.h>

#include <nx_ec/data/api_runtime_data.h>
#include <nx_ec/data/api_misc_data.h>

#include <nx/utils/thread/joinable.h>
#include <utils/common/rfc868_servers.h>
#include <nx/utils/log/log.h>
#include <nx/utils/std/cpp14.h>
#include <nx/utils/timer_manager.h>
#include <nx/network/time/mean_time_fetcher.h>
#include <nx/network/time/time_protocol_client.h>

#include <api/app_server_connection.h>
#include "ec2_thread_pool.h"
#include "rest/time_sync_rest_handler.h"
#include "settings.h"
#include "ec2_connection.h"


namespace ec2 {

/** This parameter holds difference between local system time and synchronized time. */
static const QByteArray TIME_DELTA_PARAM_NAME = "sync_time_delta";
static const QByteArray LOCAL_TIME_PRIORITY_KEY_PARAM_NAME = "time_priority_key";
/** Time priority correspinding to saved synchronised time. */
static const QByteArray USED_TIME_PRIORITY_KEY_PARAM_NAME = "used_time_priority_key";

template<class Function>
class CustomRunnable:
    public QRunnable
{
public:
    CustomRunnable( Function&& function ):
        m_function( std::move(function) )
    {
        setAutoDelete( true );
    }

    virtual void run()
    {
        m_function();
    }

private:
    Function m_function;
};

template<class Function>
CustomRunnable<Function>* make_custom_runnable( Function&& function )
{
    return new CustomRunnable<Function>( std::move( function ) );
}

/**
 * @param syncTimeToLocalDelta local_time - sync_time
 */
bool loadSyncTime(
    Ec2DirectConnectionPtr connection,
    qint64* const syncTimeToLocalDelta,
    TimePriorityKey* const syncTimeKey)
{
    if (!connection)
        return false;
    auto manager = connection->getMiscManager(Qn::kSystemAccess);

    ApiMiscData syncTimeToLocalDeltaData;
    if (manager->getMiscParamSync(
            TIME_DELTA_PARAM_NAME,
            &syncTimeToLocalDeltaData) != ErrorCode::ok)
        return false;

    ApiMiscData syncTimeKeyData;
    if (manager->getMiscParamSync(
            USED_TIME_PRIORITY_KEY_PARAM_NAME,
            &syncTimeKeyData) != ErrorCode::ok)
        return false;

    *syncTimeToLocalDelta = syncTimeToLocalDeltaData.value.toLongLong();
    syncTimeKey->fromUInt64(syncTimeKeyData.value.toULongLong());
    return true;
}


//////////////////////////////////////////////
//   TimePriorityKey
//////////////////////////////////////////////
TimePriorityKey::TimePriorityKey():
    sequence(0),
    flags(0),
    seed(0)
{
}

bool TimePriorityKey::operator==( const TimePriorityKey& right ) const
{
    return
        sequence == right.sequence &&
        flags == right.flags &&
        seed == right.seed;
}

bool TimePriorityKey::operator!=( const TimePriorityKey& right ) const
{
    return !(*this == right);
}

bool TimePriorityKey::hasLessPriorityThan(
    const TimePriorityKey& right,
    bool takeIntoAccountInternetTime) const
{
    const int peerIsServerSet = flags & Qn::TF_peerIsServer;
    const int right_peerIsServerSet = right.flags & Qn::TF_peerIsServer;
    if (peerIsServerSet < right_peerIsServerSet)
        return true;
    if (right_peerIsServerSet < peerIsServerSet)
        return false;

    if (takeIntoAccountInternetTime)
    {
        const int internetFlagSet = flags & Qn::TF_peerTimeSynchronizedWithInternetServer;
        const int right_internetFlagSet = right.flags & Qn::TF_peerTimeSynchronizedWithInternetServer;
        if (internetFlagSet < right_internetFlagSet)
            return true;
        if (right_internetFlagSet < internetFlagSet)
            return false;
    }

    if (sequence != right.sequence)
    {
        //taking into account sequence overflow. it should be same as "sequence < right.sequence"
        //but with respect to sequence overflow
        return ((quint16)(right.sequence - sequence)) <
            (std::numeric_limits<decltype(sequence)>::max() / 2);
    }

    quint16 effectiveFlags = flags;
    quint16 right_effectiveFlags = right.flags;
    if (!takeIntoAccountInternetTime)
    {
        effectiveFlags &= ~Qn::TF_peerTimeSynchronizedWithInternetServer;
        right_effectiveFlags &= ~Qn::TF_peerTimeSynchronizedWithInternetServer;
    }

    return
        effectiveFlags < right_effectiveFlags ? true :
        effectiveFlags > right_effectiveFlags ? false :
        seed < right.seed;
}


quint64 TimePriorityKey::toUInt64() const
{
    return ((quint64)sequence << 48) | ((quint64)flags << 32) | seed;
}

void TimePriorityKey::fromUInt64( quint64 val )
{
    sequence = (quint16)(val >> 48);
    flags = (quint16)((val >> 32) & 0xFFFF);
    seed = val & 0xFFFFFFFF;
}

bool TimePriorityKey::isTakenFromInternet() const
{
    return (flags & Qn::TF_peerTimeSynchronizedWithInternetServer) > 0;
}


//////////////////////////////////////////////
//   TimeSyncInfo
//////////////////////////////////////////////
TimeSyncInfo::TimeSyncInfo(
    qint64 _monotonicClockValue,
    qint64 _syncTime,
    const TimePriorityKey& _timePriorityKey )
:
    monotonicClockValue( _monotonicClockValue ),
    syncTime( _syncTime ),
    timePriorityKey( _timePriorityKey )
{
}

QByteArray TimeSyncInfo::toString() const
{
    static const size_t MAX_DECIMAL_DIGITS_IN_64BIT_INT = 20;

    QByteArray result;
    result.resize( MAX_DECIMAL_DIGITS_IN_64BIT_INT*3 + 1 );
    sprintf( result.data(), "%lld_%lld_%lld", monotonicClockValue, syncTime, timePriorityKey.toUInt64() );
    result.resize( (int)strlen( result.data() ) );
    return result;
}

bool TimeSyncInfo::fromString( const QByteArray& str )
{
    int curSepPos = 0;
    for( int i = 0; curSepPos < str.size(); ++i )
    {
        int nextSepPos = str.indexOf( '_', curSepPos );
        if( nextSepPos == -1 )
            nextSepPos = str.size();
        if( i == 0 )
            monotonicClockValue = QByteArray::fromRawData(str.constData()+curSepPos, nextSepPos-curSepPos).toLongLong();
        else if( i == 1 )
            syncTime = QByteArray::fromRawData(str.constData()+curSepPos, nextSepPos-curSepPos).toLongLong();
        else if( i == 2 )
            timePriorityKey.fromUInt64( QByteArray::fromRawData(str.constData()+curSepPos, nextSepPos-curSepPos).toLongLong() );

        curSepPos = nextSepPos+1;
    }
    return true;
}


//#define TEST_PTS_SELECTION


//////////////////////////////////////////////
//   TimeSynchronizationManager
//////////////////////////////////////////////
static const size_t MILLIS_PER_SEC = 1000;
static const size_t INITIAL_INTERNET_SYNC_TIME_PERIOD_SEC = 5;
static const size_t MIN_INTERNET_SYNC_TIME_PERIOD_SEC = 60;
#ifdef _DEBUG
static const size_t LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS = 10*MILLIS_PER_SEC;
static const size_t MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS = 60*MILLIS_PER_SEC;
static const size_t INTERNET_SYNC_TIME_PERIOD_SEC = 60;
/** Reporting time synchronization information to other peers once per this period. */
static const int TIME_SYNC_SEND_TIMEOUT_SEC = 10;
#else
static const size_t LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS = 10*60*MILLIS_PER_SEC;
/** Once per 10 minutes checking if manual time server selection is required. */
static const size_t MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS = 10*60*MILLIS_PER_SEC;
/** Accurate time is fetched from internet with this period. */
static const size_t INTERNET_SYNC_TIME_PERIOD_SEC = 60*60;
/** Reporting time synchronization information to other peers once per this period. */
static const int TIME_SYNC_SEND_TIMEOUT_SEC = 10*60;
#endif
/**
 * If time synchronization with internet failes, period is multiplied on this value,
 * but it cannot exceed MAX_PUBLIC_SYNC_TIME_PERIOD_SEC.
 */
static const size_t INTERNET_SYNC_TIME_FAILURE_PERIOD_GROW_COEFF = 2;
static const size_t MAX_INTERNET_SYNC_TIME_PERIOD_SEC = 60*60;
//static const size_t INTERNET_TIME_EXPIRATION_PERIOD_SEC = 7*24*60*60;   //one week
/** Considering internet time equal to local time if difference is no more than this value. */
static const qint64 MAX_LOCAL_SYSTEM_TIME_DRIFT_MS = 10*MILLIS_PER_SEC;
static const int MAX_SEQUENT_INTERNET_SYNCHRONIZATION_FAILURES = 15;
static const int MAX_RTT_TIME_MS = 10 * 1000;
/** Maximum time drift between servers that we want to keep up to. */
static const int MAX_DESIRED_TIME_DRIFT_MS = 1000;
/**
 * Considering that other server's time is retrieved with error not less then MIN_GET_TIME_ERROR_MS.
 * This should help against redundant clock resync.
 */
static const int MIN_GET_TIME_ERROR_MS = 100;
/** Once per this interval we check if local OS time has been changed. */
static const int SYSTEM_TIME_CHANGE_CHECK_PERIOD_MS = 10 * MILLIS_PER_SEC;
/** This coefficient applied to request round-trip time when comparing sync time from different servers. */
static const int SYNC_TIME_DRIFT_MAX_ERROR_COEFF = 5;

static_assert( MIN_INTERNET_SYNC_TIME_PERIOD_SEC > 0, "MIN_INTERNET_SYNC_TIME_PERIOD_SEC MUST be > 0!" );
static_assert( MIN_INTERNET_SYNC_TIME_PERIOD_SEC <= MAX_INTERNET_SYNC_TIME_PERIOD_SEC,
    "Check MIN_INTERNET_SYNC_TIME_PERIOD_SEC and MAX_INTERNET_SYNC_TIME_PERIOD_SEC" );
static_assert( INTERNET_SYNC_TIME_PERIOD_SEC >= MIN_INTERNET_SYNC_TIME_PERIOD_SEC,
    "Check INTERNET_SYNC_TIME_PERIOD_SEC and MIN_INTERNET_SYNC_TIME_PERIOD_SEC" );
static_assert( INTERNET_SYNC_TIME_PERIOD_SEC <= MAX_INTERNET_SYNC_TIME_PERIOD_SEC,
    "Check INTERNET_SYNC_TIME_PERIOD_SEC and MAX_INTERNET_SYNC_TIME_PERIOD_SEC" );

TimeSynchronizationManager::TimeSynchronizationManager(
    Qn::PeerType peerType,
    nx::utils::TimerManager* const timerManager,
    AbstractTransactionMessageBus* messageBus,
    Settings* settings)
:
    m_localSystemTimeDelta( std::numeric_limits<qint64>::min() ),
    m_internetSynchronizationTaskID( 0 ),
    m_manualTimerServerSelectionCheckTaskID( 0 ),
    m_checkSystemTimeTaskID( 0 ),
    m_terminated( false ),
    m_messageBus(messageBus),
    m_peerType( peerType ),
    m_timerManager(timerManager),
    m_internetTimeSynchronizationPeriod( INITIAL_INTERNET_SYNC_TIME_PERIOD_SEC ),
    m_timeSynchronized( false ),
    m_internetSynchronizationFailureCount( 0 ),
    m_settings( settings ),
    m_asyncOperationsInProgress( 0 )
{
}

TimeSynchronizationManager::~TimeSynchronizationManager()
{
    directDisconnectAll();

    pleaseStop();
}

void TimeSynchronizationManager::pleaseStop()
{
    quint64 manualTimerServerSelectionCheckTaskID = 0;
    quint64 internetSynchronizationTaskID = 0;
    quint64 checkSystemTimeTaskID = 0;
    {
        QnMutexLocker lock( &m_mutex );
        m_terminated = true;

        manualTimerServerSelectionCheckTaskID = m_manualTimerServerSelectionCheckTaskID;
        m_manualTimerServerSelectionCheckTaskID = 0;

        internetSynchronizationTaskID = m_internetSynchronizationTaskID;
        m_internetSynchronizationTaskID = 0;

        checkSystemTimeTaskID = m_checkSystemTimeTaskID;
        m_checkSystemTimeTaskID = 0;
    }

    if( manualTimerServerSelectionCheckTaskID )
        m_timerManager->joinAndDeleteTimer( manualTimerServerSelectionCheckTaskID );

    if( internetSynchronizationTaskID )
        m_timerManager->joinAndDeleteTimer( internetSynchronizationTaskID );

    if( checkSystemTimeTaskID )
        m_timerManager->joinAndDeleteTimer( checkSystemTimeTaskID );

    if( m_timeSynchronizer )
    {
        m_timeSynchronizer->pleaseStopSync();
        m_timeSynchronizer.reset();
    }

    QnMutexLocker lock( &m_mutex );
    while( m_asyncOperationsInProgress )
        m_asyncOperationsWaitCondition.wait( lock.mutex() );
}

void TimeSynchronizationManager::start(const std::shared_ptr<Ec2DirectConnection>& connection)
{
    m_connection = connection;

    if( m_peerType == Qn::PT_Server )
        m_localTimePriorityKey.flags |= Qn::TF_peerIsServer;

#ifndef EDGE_SERVER
    m_localTimePriorityKey.flags |= Qn::TF_peerIsNotEdgeServer;
#endif
    QByteArray localGUID = m_messageBus->commonModule()->moduleGUID().toByteArray();
    m_localTimePriorityKey.seed = crc32(0, (const Bytef*)localGUID.constData(), localGUID.size());
    //TODO #ak use guid to avoid handle priority key duplicates
    if( QElapsedTimer::isMonotonic() )
        m_localTimePriorityKey.flags |= Qn::TF_peerHasMonotonicClock;

    m_monotonicClock.restart();
    //initializing synchronized time with local system time
    m_usedTimeSyncInfo = TimeSyncInfo(
        0,
        currentMSecsSinceEpoch(),
        m_localTimePriorityKey );

    if (m_connection)
        onDbManagerInitialized();

    connect(m_messageBus, &AbstractTransactionMessageBus::newDirectConnectionEstablished,
        this, &TimeSynchronizationManager::onNewConnectionEstablished,
        Qt::DirectConnection);

    connect(m_messageBus, &QnTransactionMessageBus::peerLost,
                this, &TimeSynchronizationManager::onPeerLost,
                Qt::DirectConnection );
    const auto& commonModule = m_messageBus->commonModule();
    Qn::directConnect(commonModule->globalSettings(), &QnGlobalSettings::timeSynchronizationSettingsChanged,
        this, &TimeSynchronizationManager::onTimeSynchronizationSettingsChanged);

    {
        QnMutexLocker lock( &m_mutex );

        using namespace std::placeholders;
        if( m_peerType == Qn::PT_Server )
        {
            initializeTimeFetcher();
            addInternetTimeSynchronizationTask();

            m_checkSystemTimeTaskID = m_timerManager->addTimer(
                std::bind(&TimeSynchronizationManager::checkSystemTimeForChange, this),
                std::chrono::milliseconds(SYSTEM_TIME_CHANGE_CHECK_PERIOD_MS));
        }
        else
        {
            //m_manualTimerServerSelectionCheckTaskID = m_timerManager->addTimer(
            //    std::bind( &TimeSynchronizationManager::checkIfManualTimeServerSelectionIsRequired, this, _1 ),
            //    std::chrono::milliseconds(MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS));
        }
    }
}

qint64 TimeSynchronizationManager::getSyncTime() const
{
    QnMutexLocker lock( &m_mutex );
    return getSyncTimeNonSafe();
}

ApiTimeData TimeSynchronizationManager::getTimeInfo() const
{
    std::vector<QnUuid> allServerIds;
    if (const auto& resourcePool = m_messageBus->commonModule()->resourcePool())
    {
        for (const auto& server: resourcePool->getAllServers(Qn::AnyStatus))
            allServerIds.push_back(server->getId());
    }

    QnMutexLocker lock(&m_mutex);
    ApiTimeData result;
    result.value = getSyncTimeNonSafe();
    result.isPrimaryTimeServer = m_usedTimeSyncInfo.timePriorityKey == m_localTimePriorityKey;

    if (m_usedTimeSyncInfo.timePriorityKey.flags & Qn::TF_peerTimeSynchronizedWithInternetServer)
        result.syncTimeFlags |= Qn::TF_peerTimeSynchronizedWithInternetServer;
    if (m_usedTimeSyncInfo.timePriorityKey.flags & Qn::TF_peerTimeSetByUser)
        result.syncTimeFlags |= Qn::TF_peerTimeSetByUser;

    for (const auto& serverId: allServerIds)
    {
        const auto s = serverId.toByteArray();
        if (m_usedTimeSyncInfo.timePriorityKey.seed == crc32(0, (const Bytef*) s.constData(), s.size()))
        {
            result.primaryTimeServerGuid = serverId;
            break;
        }
    }

    return result;
}

void TimeSynchronizationManager::onGotPrimariTimeServerTran(const QnTransaction<ApiIdData>& tran)
{
    primaryTimeServerChanged(tran.params.id);
}

void TimeSynchronizationManager::primaryTimeServerChanged(const ApiIdData& serverId)
{
    quint64 localTimePriorityBak = 0;
    quint64 newLocalTimePriority = 0;
    {
        QnMutexLocker lock( &m_mutex );

        localTimePriorityBak = m_localTimePriorityKey.toUInt64();
        const auto& commonModule = m_messageBus->commonModule();

        NX_LOGX( lit("Received primary time server change transaction. new peer %1, local peer %2").
            arg(serverId.id.toString() ).arg(commonModule->moduleGUID().toString() ), cl_logDEBUG1 );

        if(serverId.id == commonModule->moduleGUID() )
        {
            m_localTimePriorityKey.flags |= Qn::TF_peerTimeSetByUser;
            selectLocalTimeAsSynchronized(&lock, m_usedTimeSyncInfo.timePriorityKey.sequence + 1);
        }
        else
        {
            m_localTimePriorityKey.flags &= ~Qn::TF_peerTimeSetByUser;
        }
        newLocalTimePriority = m_localTimePriorityKey.toUInt64();
        if (newLocalTimePriority != localTimePriorityBak)
            handleLocalTimePriorityKeyChange(&lock);
    }

    /* Can cause signal, going out of mutex locker. */
    if( newLocalTimePriority != localTimePriorityBak )
        updateRuntimeInfoPriority(newLocalTimePriority);
}

void TimeSynchronizationManager::selectLocalTimeAsSynchronized(
    QnMutexLockerBase* const lock,
    quint16 newTimePriorityKeySequence)
{
    const auto& commonModule = m_messageBus->commonModule();

    //local peer is selected by user as primary time server
    const bool synchronizingByCurrentServer = m_usedTimeSyncInfo.timePriorityKey == m_localTimePriorityKey;
    //incrementing sequence
    m_localTimePriorityKey.sequence = newTimePriorityKeySequence;
    //"select primary time server" means "take its local time", so resetting internet synchronization flag
    m_localTimePriorityKey.flags &= ~Qn::TF_peerTimeSynchronizedWithInternetServer;
    if (!m_usedTimeSyncInfo.timePriorityKey.hasLessPriorityThan(
        	m_localTimePriorityKey,
        	commonModule->globalSettings()->isSynchronizingTimeWithInternet()))
    {
        return;
    }

    //using current server time info
    const qint64 elapsed = m_monotonicClock.elapsed();
    //selection of peer as primary time server means it's local system time is to be used as synchronized time
    //in case of internet connection absence
    m_usedTimeSyncInfo = TimeSyncInfo(
        elapsed,
        currentMSecsSinceEpoch(),
        m_localTimePriorityKey);
    m_timeSynchronized = true;

    NX_LOGX(lit("Selecting local server time as synchronized. "
            "New synchronized time %1, new priority key 0x%2")
        .arg(QDateTime::fromMSecsSinceEpoch(m_usedTimeSyncInfo.syncTime).toString(Qt::ISODate))
        .arg(m_localTimePriorityKey.toUInt64(), 0, 16), cl_logINFO);

    //saving synchronized time to DB
    if (m_connection)
    {
        saveSyncTimeAsync(
            lock,
            0,
            m_usedTimeSyncInfo.timePriorityKey);
    }

    if (!synchronizingByCurrentServer)
    {
        const qint64 curSyncTime = m_usedTimeSyncInfo.syncTime;
        lock->unlock();
        WhileExecutingDirectCall callGuard(this);
        emit timeChanged(curSyncTime);
        lock->relock();
    }

    //reporting new sync time to everyone we know
    syncTimeWithAllKnownServers(*lock);
}

TimeSyncInfo TimeSynchronizationManager::getTimeSyncInfo() const
{
    QnMutexLocker lock( &m_mutex );
    return getTimeSyncInfoNonSafe();
}

qint64 TimeSynchronizationManager::getMonotonicClock() const
{
    QnMutexLocker lock( &m_mutex );
    return m_monotonicClock.elapsed();
}

void TimeSynchronizationManager::forgetSynchronizedTime()
{
    QnMutexLocker lock( &m_mutex );
    forgetSynchronizedTimeNonSafe(&lock);
}

void TimeSynchronizationManager::forceTimeResync()
{
    {
        QnMutexLocker lock(&m_mutex);
        syncTimeWithAllKnownServers(lock);
        forgetSynchronizedTimeNonSafe(&lock);
    }

    const auto curSyncTime = getSyncTime();
    WhileExecutingDirectCall callGuard(this);
    emit timeChanged(curSyncTime);
}

void TimeSynchronizationManager::processTimeSyncInfoHeader(
    const QnUuid& peerID,
    const nx_http::StringType& serializedTimeSync,
    boost::optional<qint64> rttMillis)
{
    TimeSyncInfo remotePeerTimeSyncInfo;
    if( !remotePeerTimeSyncInfo.fromString( serializedTimeSync ) )
        return;

    NX_ASSERT( remotePeerTimeSyncInfo.timePriorityKey.seed > 0 );

    QnMutexLocker lock( &m_mutex );
    const auto localTimePriorityBak = m_localTimePriorityKey.toUInt64();
    remotePeerTimeSyncUpdate(
        &lock,
        peerID,
        m_monotonicClock.elapsed(),
        remotePeerTimeSyncInfo.syncTime + (rttMillis ? (rttMillis.get() / 2) : 0),
        remotePeerTimeSyncInfo.timePriorityKey,
        rttMillis ? rttMillis.get() : MAX_DESIRED_TIME_DRIFT_MS);
    if (m_localTimePriorityKey.toUInt64() != localTimePriorityBak)
        handleLocalTimePriorityKeyChange(&lock);
}

void TimeSynchronizationManager::remotePeerTimeSyncUpdate(
    QnMutexLockerBase* const lock,
    const QnUuid& remotePeerID,
    qint64 localMonotonicClock,
    qint64 remotePeerSyncTime,
    const TimePriorityKey& remotePeerTimePriorityKey,
    const qint64 timeErrorEstimation )
{
    const auto& commonModule = m_messageBus->commonModule();
    NX_ASSERT( remotePeerTimePriorityKey.seed > 0 );
    NX_LOGX( QString::fromLatin1("Received sync time update from peer %1, "
        "peer's sync time (%2), peer's time priority key 0x%3. Local peer id %4, local sync time %5, used priority key 0x%6").
        arg(remotePeerID.toString()).arg(QDateTime::fromMSecsSinceEpoch(remotePeerSyncTime).toString(Qt::ISODate)).
        arg(remotePeerTimePriorityKey.toUInt64(), 0, 16).arg(commonModule->moduleGUID().toString()).
        arg(QDateTime::fromMSecsSinceEpoch(getSyncTimeNonSafe()).toString(Qt::ISODate)).
        arg(m_usedTimeSyncInfo.timePriorityKey.toUInt64(), 0, 16), cl_logDEBUG2 );

    //time difference between this server and remote one is not that great
    const auto timeDifference = remotePeerSyncTime - getSyncTimeNonSafe();
    const auto effectiveTimeErrorEstimation =
        std::max<qint64>(timeErrorEstimation * SYNC_TIME_DRIFT_MAX_ERROR_COEFF, MIN_GET_TIME_ERROR_MS);
    const bool maxTimeDriftExceeded = std::abs(timeDifference) >= effectiveTimeErrorEstimation;
    const bool needAdjustClockDueToLargeDrift =
        //taking drift into account if both servers have time with same key
        (remotePeerTimePriorityKey == m_usedTimeSyncInfo.timePriorityKey) &&
        maxTimeDriftExceeded &&
        (remotePeerID > commonModule->moduleGUID());

    //if there is new maximum remotePeerTimePriorityKey then updating delta and emitting timeChanged
    if( !(m_usedTimeSyncInfo.timePriorityKey.hasLessPriorityThan(
            remotePeerTimePriorityKey, commonModule->globalSettings()->isSynchronizingTimeWithInternet())) &&
        !needAdjustClockDueToLargeDrift )
    {
        return; //not applying time
    }

    // If Internet time has been reported and synchronizing with local peer, then taking local time once again.
    if (remotePeerTimePriorityKey.isTakenFromInternet() &&
        !commonModule->globalSettings()->isSynchronizingTimeWithInternet() &&
        ((m_localTimePriorityKey.flags & Qn::TF_peerTimeSetByUser) > 0))
    {
        // Sending back local time with increased sequence.
        NX_LOG(lm("TimeSynchronizationManager. Received Internet time "
            "while user enabled synchronization with local peer. "
            "Increasing local time priority"), cl_logDEBUG1);
        selectLocalTimeAsSynchronized(lock, remotePeerTimePriorityKey.sequence + 1);
        return;
    }

    //printing sync time change reason to the log
    if (!(m_usedTimeSyncInfo.timePriorityKey.hasLessPriorityThan(
            remotePeerTimePriorityKey, commonModule->globalSettings()->isSynchronizingTimeWithInternet())) &&
        needAdjustClockDueToLargeDrift)
    {
        NX_LOGX(lm("Received sync time update from peer %1, peer's sync time (%2). "
            "Accepting peer's synchronized time due to large drift (%3 ms, fault %4)").
            arg(remotePeerID.toString()).arg(QDateTime::fromMSecsSinceEpoch(remotePeerSyncTime).toString(Qt::ISODate)).
            arg(timeDifference).arg(effectiveTimeErrorEstimation), cl_logDEBUG1);
    }
    else
    {
        NX_LOGX(lm("Received sync time update from peer %1, peer's sync time (%2), "
            "peer's time priority key 0x%3. Local peer id %4, local sync time %5, used priority key 0x%6. Accepting peer's synchronized time").
            arg(remotePeerID.toString()).arg(QDateTime::fromMSecsSinceEpoch(remotePeerSyncTime).toString(Qt::ISODate)).
            arg(remotePeerTimePriorityKey.toUInt64(), 0, 16).arg(commonModule->moduleGUID().toString()).
            arg(QDateTime::fromMSecsSinceEpoch(getSyncTimeNonSafe()).toString(Qt::ISODate)).
            arg(m_usedTimeSyncInfo.timePriorityKey.toUInt64(), 0, 16), cl_logDEBUG1);
    }

    //taking new synchronization data
    m_usedTimeSyncInfo = TimeSyncInfo(
        localMonotonicClock,
        remotePeerSyncTime,
        remotePeerTimePriorityKey );
    if( needAdjustClockDueToLargeDrift )
    {
        const auto isTimeSynchronizedByThisServer =
            m_localTimePriorityKey.seed == m_usedTimeSyncInfo.timePriorityKey.seed;
        ++m_usedTimeSyncInfo.timePriorityKey.sequence;   //for case if synchronizing because of time drift
        if (isTimeSynchronizedByThisServer)
            m_localTimePriorityKey.sequence = m_usedTimeSyncInfo.timePriorityKey.sequence;
    }
    const qint64 curSyncTime = m_usedTimeSyncInfo.syncTime + m_monotonicClock.elapsed() - m_usedTimeSyncInfo.monotonicClockValue;
    //saving synchronized time to DB
    m_timeSynchronized = true;
    if (m_connection)
    {
        saveSyncTimeAsync(
            lock,
            QDateTime::currentMSecsSinceEpoch() - curSyncTime,
            m_usedTimeSyncInfo.timePriorityKey);
    }
    lock->unlock();
    {
        WhileExecutingDirectCall callGuard( this );
        emit timeChanged( curSyncTime );
    }
    lock->relock();

    //informing all servers we can about time change as soon as possible
    syncTimeWithAllKnownServers(*lock);
}

void TimeSynchronizationManager::onNewConnectionEstablished(QnAbstractTransactionTransport* transport )
{
    using namespace std::placeholders;

    if (transport->remotePeer().peerType != Qn::PT_Server)
        return;
#if 0
    if( transport->isIncoming() )
    {
        //peer connected to us
        //using transactions to signal remote peer that it needs to fetch time from us
        transport->setBeforeSendingChunkHandler(
            std::bind(&TimeSynchronizationManager::onBeforeSendingTransaction, this, _1, _2));
    }
    else
    {
        //listening time change signals fom remote peer which cannot connect to us
        transport->setHttpChunkExtensonHandler(
            std::bind(&TimeSynchronizationManager::onTransactionReceived, this, _1, _2));
    }
#endif
    if (!transport->isIncoming())
    {
        //we can connect to the peer
        const QUrl remoteAddr = transport->remoteAddr();
        //saving credentials has been used to establish connection
        startSynchronizingTimeWithPeer( //starting sending time sync info to peer
            transport->remotePeer().id,
            SocketAddress( remoteAddr.host(), remoteAddr.port() ),
            transport->authData() );
    }
}

void TimeSynchronizationManager::onPeerLost(QnUuid peer, Qn::PeerType /*peerType*/)
{
    stopSynchronizingTimeWithPeer(peer);
}

void TimeSynchronizationManager::startSynchronizingTimeWithPeer(
    const QnUuid& peerID,
    SocketAddress peerAddress,
    nx_http::AuthInfoCache::AuthorizationCacheItem authData )
{
    QnMutexLocker lock( &m_mutex );
    auto iterResultPair = m_peersToSendTimeSyncTo.emplace(
        peerID,
        PeerContext( std::move(peerAddress), std::move(authData) ) );
    if( !iterResultPair.second )
        return; //already exists
    PeerContext& ctx = iterResultPair.first->second;
    //adding periodic task
    ctx.syncTimerID = nx::utils::TimerManager::TimerGuard(
        m_timerManager,
        m_timerManager->addTimer(
            std::bind(&TimeSynchronizationManager::synchronizeWithPeer, this, peerID),
            std::chrono::milliseconds::zero()));    //performing initial synchronization immediately
}

void TimeSynchronizationManager::stopSynchronizingTimeWithPeer( const QnUuid& peerID )
{
    nx::utils::TimerManager::TimerGuard timerID;
    nx_http::AsyncHttpClientPtr httpClient;

    QnMutexLocker lock( &m_mutex );
    auto peerIter = m_peersToSendTimeSyncTo.find( peerID );
    if( peerIter == m_peersToSendTimeSyncTo.end() )
        return;
    timerID = std::move(peerIter->second.syncTimerID);
    httpClient = std::move(peerIter->second.httpClient);
    m_peersToSendTimeSyncTo.erase( peerIter );

    //timerID and httpClient will be destroyed after mutex unlock
}

void TimeSynchronizationManager::synchronizeWithPeer( const QnUuid& peerID )
{
    const auto& commonModule = m_messageBus->commonModule();
    nx_http::AsyncHttpClientPtr clientPtr;

    QnMutexLocker lock( &m_mutex );
    if( m_terminated )
        return;

    auto peerIter = m_peersToSendTimeSyncTo.find( peerID );
    if( peerIter == m_peersToSendTimeSyncTo.end() )
    {
        NX_LOG(lm("TimeSynchronizationManager. Cannot report sync_time to peer %1. "
            "Address not known...").arg(peerID), cl_logDEBUG2);
        return;
    }

    if (!commonModule->globalSettings()->isTimeSynchronizationEnabled())
    {
            peerIter->second.syncTimerID =
            nx::utils::TimerManager::TimerGuard(
                m_timerManager,
                m_timerManager->addTimer(
                    std::bind(&TimeSynchronizationManager::synchronizeWithPeer, this, peerID),
                    std::chrono::milliseconds(TIME_SYNC_SEND_TIMEOUT_SEC * MILLIS_PER_SEC)));
        return;
    }

    NX_LOG(lm("TimeSynchronizationManager. About to report sync_time to peer %1 (%2)")
        .arg(peerID).arg(peerIter->second.peerAddress.toString()), cl_logDEBUG2);

    QUrl targetUrl;
    targetUrl.setScheme( lit("http") );
    targetUrl.setHost( peerIter->second.peerAddress.address.toString() );
    targetUrl.setPort( peerIter->second.peerAddress.port );
    targetUrl.setPath( lit("/") + QnTimeSyncRestHandler::PATH );

    NX_ASSERT( !peerIter->second.httpClient );
    clientPtr = nx_http::AsyncHttpClient::create();
    using namespace std::placeholders;
    const auto requestSendClock = m_monotonicClock.elapsed();
    connect(
        clientPtr.get(), &nx_http::AsyncHttpClient::done,
        this,
        [this, peerID, requestSendClock]( nx_http::AsyncHttpClientPtr clientPtr ) {
            const qint64 rtt = m_monotonicClock.elapsed() - requestSendClock;
            {
                //saving rtt
                QnMutexLocker lock(&m_mutex);
                auto peerIter = m_peersToSendTimeSyncTo.find(peerID);
                if (peerIter != m_peersToSendTimeSyncTo.end())
                    peerIter->second.rttMillis = rtt;
            }
            timeSyncRequestDone(
                peerID,
                std::move(clientPtr),
                rtt);
        },
        Qt::DirectConnection );

    if( m_peerType == Qn::PT_Server )
    {
        //client does not send its time to anyone
        const auto timeSyncInfo = getTimeSyncInfoNonSafe();
        clientPtr->addAdditionalHeader(
            QnTimeSyncRestHandler::TIME_SYNC_HEADER_NAME,
            timeSyncInfo.toString() );
    }
    clientPtr->addAdditionalHeader( Qn::PEER_GUID_HEADER_NAME, commonModule->moduleGUID().toByteArray() );
    if (peerIter->second.rttMillis)
    {
        clientPtr->addAdditionalHeader(
            Qn::RTT_MS_HEADER_NAME,
            nx_http::StringType::number(peerIter->second.rttMillis.get()));
    }

    clientPtr->setUserName( peerIter->second.authData.userName );
    if( peerIter->second.authData.password )
    {
        clientPtr->setAuthType( nx_http::AsyncHttpClient::authBasicAndDigest );
        clientPtr->setUserPassword( peerIter->second.authData.password.get() );
    }
    else if( peerIter->second.authData.ha1 )
    {
        clientPtr->setAuthType( nx_http::AsyncHttpClient::authDigestWithPasswordHash );
        clientPtr->setUserPassword( peerIter->second.authData.ha1.get() );
    }

    clientPtr->doGet( targetUrl );
    peerIter->second.syncTimerID.reset();
    peerIter->second.httpClient.swap( clientPtr );
    //clientPtr will be destroyed after mutex unlock
}

void TimeSynchronizationManager::timeSyncRequestDone(
    const QnUuid& peerID,
    nx_http::AsyncHttpClientPtr clientPtr,
    qint64 requestRttMillis)
{
    NX_LOG(lm("TimeSynchronizationManager. Received (%1) response from peer %2")
        .arg(clientPtr->response() ? clientPtr->response()->statusLine.statusCode : -1)
        .arg(peerID), cl_logDEBUG2);

    if( clientPtr->response() &&
        clientPtr->response()->statusLine.statusCode == nx_http::StatusCode::ok )
    {
        //reading time sync information from remote server
        auto timeSyncHeaderIter = clientPtr->response()->headers.find( QnTimeSyncRestHandler::TIME_SYNC_HEADER_NAME );
        if( timeSyncHeaderIter != clientPtr->response()->headers.end() )
        {
            auto sock = clientPtr->takeSocket();
            processTimeSyncInfoHeader(
                peerID,
                timeSyncHeaderIter->second,
                requestRttMillis);
        }
    }

    QnMutexLocker lock( &m_mutex );
    auto peerIter = m_peersToSendTimeSyncTo.find( peerID );
    if( peerIter == m_peersToSendTimeSyncTo.end() ||
        peerIter->second.httpClient != clientPtr )
    {
        return;
    }
    NX_ASSERT( !peerIter->second.syncTimerID );
    peerIter->second.httpClient.reset();
    //scheduling next synchronization
    if( m_terminated )
        return;
        peerIter->second.syncTimerID =
        nx::utils::TimerManager::TimerGuard(
            m_timerManager,
            m_timerManager->addTimer(
                std::bind( &TimeSynchronizationManager::synchronizeWithPeer, this, peerID ),
                std::chrono::milliseconds(TIME_SYNC_SEND_TIMEOUT_SEC * MILLIS_PER_SEC)));
}

void TimeSynchronizationManager::checkIfManualTimeServerSelectionIsRequired( quint64 /*taskID*/ )
{
    //TODO #ak it is better to run this method on event, not by timeout
    QnMutexLocker lock( &m_mutex );

    m_manualTimerServerSelectionCheckTaskID = 0;
    if( m_terminated )
        return;

    using namespace std::placeholders;
    m_manualTimerServerSelectionCheckTaskID = m_timerManager->addTimer(
        std::bind( &TimeSynchronizationManager::checkIfManualTimeServerSelectionIsRequired, this, _1 ),
        std::chrono::milliseconds(MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS));

    if (m_usedTimeSyncInfo.timePriorityKey == m_localTimePriorityKey)
        return;
    if (m_usedTimeSyncInfo.timePriorityKey.flags & Qn::TF_peerTimeSynchronizedWithInternetServer)
        return;
    if (m_usedTimeSyncInfo.timePriorityKey.flags & Qn::TF_peerTimeSetByUser)
        return;

    NX_LOGX(lm("User input required. Used sync time 0x%1, local time 0x%2")
        .arg(m_usedTimeSyncInfo.timePriorityKey.toUInt64(), 0, 16)
        .arg(m_localTimePriorityKey.toUInt64(), 0, 16),
        cl_logDEBUG2);
}


void TimeSynchronizationManager::syncTimeWithInternet( quint64 taskID )
{
    using namespace std::placeholders;

    NX_LOGX( lit( "TimeSynchronizationManager::syncTimeWithInternet. taskID %1" ).arg( taskID ), cl_logDEBUG2 );
    const auto& commonModule = m_messageBus->commonModule();
    const bool isSynchronizingTimeWithInternet =
        commonModule->globalSettings()->isSynchronizingTimeWithInternet();

    QnMutexLocker lock( &m_mutex );

    if( (taskID != m_internetSynchronizationTaskID) || m_terminated )
        return;
    m_internetSynchronizationTaskID = 0;

    if (!isSynchronizingTimeWithInternet)
    {
        NX_LOG(lit("TimeSynchronizationManager. Not synchronizing time with internet"), cl_logDEBUG2);
            m_internetTimeSynchronizationPeriod =
            m_settings->internetSyncTimePeriodSec(INTERNET_SYNC_TIME_PERIOD_SEC);
        addInternetTimeSynchronizationTask();
        return;
    }

    NX_LOGX( lit("TimeSynchronizationManager. Synchronizing time with internet"), cl_logDEBUG1 );

    m_timeSynchronizer->getTimeAsync(
        std::bind(
            &TimeSynchronizationManager::onTimeFetchingDone,
            this, _1, _2));
}

void TimeSynchronizationManager::onTimeFetchingDone( const qint64 millisFromEpoch, SystemError::ErrorCode errorCode )
{
    using namespace std::chrono;

    quint64 localTimePriorityBak = 0;
    quint64 newLocalTimePriority = 0;
    {
        QnMutexLocker lock( &m_mutex );

        localTimePriorityBak = m_localTimePriorityKey.toUInt64();

        if( millisFromEpoch > 0 )
        {
            NX_LOGX( lit("Received time %1 from the internet").
                arg(QDateTime::fromMSecsSinceEpoch(millisFromEpoch).toString(Qt::ISODate)), cl_logDEBUG1 );

            m_internetSynchronizationFailureCount = 0;

            m_internetTimeSynchronizationPeriod = m_settings->internetSyncTimePeriodSec(INTERNET_SYNC_TIME_PERIOD_SEC);

            const qint64 curLocalTime = currentMSecsSinceEpoch();

            //using received time
            const auto localTimePriorityKeyBak = m_localTimePriorityKey;
            m_localTimePriorityKey.flags |= Qn::TF_peerTimeSynchronizedWithInternetServer;

            const auto maxDifferenceBetweenSynchronizedAndInternetTime =
                m_messageBus->commonModule()->globalSettings()->maxDifferenceBetweenSynchronizedAndInternetTime();

            if( llabs(getSyncTimeNonSafe() - millisFromEpoch) >
                duration_cast<milliseconds>(
                    maxDifferenceBetweenSynchronizedAndInternetTime).count() )
            {
                //TODO #ak use rtt here instead of constant?
                //considering synchronized time as inaccurate, even if it is marked as received from internet
                m_localTimePriorityKey.sequence = m_usedTimeSyncInfo.timePriorityKey.sequence + 1;
            }

            if( (llabs(curLocalTime - millisFromEpoch) > MAX_LOCAL_SYSTEM_TIME_DRIFT_MS) ||
                (localTimePriorityKeyBak != m_localTimePriorityKey) )
            {
                m_localSystemTimeDelta = millisFromEpoch - m_monotonicClock.elapsed();

                remotePeerTimeSyncUpdate(
                    &lock,
                    m_messageBus->commonModule()->moduleGUID(),
                    m_monotonicClock.elapsed(),
                    millisFromEpoch,
                    m_localTimePriorityKey,
                    duration_cast<milliseconds>(
                        maxDifferenceBetweenSynchronizedAndInternetTime).count() );
            }
        }
        else
        {
            NX_LOGX( lit("Failed to get time from the internet. %1")
                .arg(SystemError::toString(errorCode)), cl_logDEBUG1 );

            m_internetTimeSynchronizationPeriod = std::min<>(
                MIN_INTERNET_SYNC_TIME_PERIOD_SEC + m_internetTimeSynchronizationPeriod * INTERNET_SYNC_TIME_FAILURE_PERIOD_GROW_COEFF,
                m_settings->maxInternetTimeSyncRetryPeriodSec(MAX_INTERNET_SYNC_TIME_PERIOD_SEC));

            ++m_internetSynchronizationFailureCount;
            if( m_internetSynchronizationFailureCount > MAX_SEQUENT_INTERNET_SYNCHRONIZATION_FAILURES )
                m_localTimePriorityKey.flags &= ~Qn::TF_peerTimeSynchronizedWithInternetServer;
        }

        addInternetTimeSynchronizationTask();

        newLocalTimePriority = m_localTimePriorityKey.toUInt64();
        if (newLocalTimePriority != localTimePriorityBak)
            handleLocalTimePriorityKeyChange(&lock);
    }

    /* Can cause signal, going out of mutex locker. */
    if( newLocalTimePriority != localTimePriorityBak )
        updateRuntimeInfoPriority(newLocalTimePriority);
}

void TimeSynchronizationManager::initializeTimeFetcher()
{
    auto meanTimerFetcher = std::make_unique<nx::network::MeanTimeFetcher>();
    for (const char* timeServer: RFC868_SERVERS)
    {
        meanTimerFetcher->addTimeFetcher(
            std::make_unique<nx::network::TimeProtocolClient>(
                QLatin1String(timeServer)));
    }

    m_timeSynchronizer = std::move(meanTimerFetcher);
}

void TimeSynchronizationManager::addInternetTimeSynchronizationTask()
{
    NX_ASSERT( m_internetSynchronizationTaskID == 0 );

    if( m_terminated )
        return;

    using namespace std::placeholders;
    m_internetSynchronizationTaskID = m_timerManager->addTimer(
        std::bind( &TimeSynchronizationManager::syncTimeWithInternet, this, _1 ),
        std::chrono::milliseconds(m_internetTimeSynchronizationPeriod * MILLIS_PER_SEC));
    NX_LOGX( lit( "Added internet time sync task %1, delay %2" ).
        arg( m_internetSynchronizationTaskID ).arg( m_internetTimeSynchronizationPeriod * MILLIS_PER_SEC ), cl_logDEBUG2 );
}

qint64 TimeSynchronizationManager::currentMSecsSinceEpoch() const
{
    return m_localSystemTimeDelta == std::numeric_limits<qint64>::min()
        ? QDateTime::currentMSecsSinceEpoch()
        : m_monotonicClock.elapsed() + m_localSystemTimeDelta;
}

void TimeSynchronizationManager::updateRuntimeInfoPriority(quint64 priority)
{
    const auto& commonModule = m_messageBus->commonModule();
    QnPeerRuntimeInfo localInfo = commonModule->runtimeInfoManager()->localInfo();
    if (localInfo.data.peer.peerType != Qn::PT_Server)
        return;

    if (localInfo.data.serverTimePriority == priority)
        return;

    localInfo.data.serverTimePriority = priority;
    commonModule->runtimeInfoManager()->updateLocalItem(localInfo);
}

qint64 TimeSynchronizationManager::getSyncTimeNonSafe() const
{
    return m_usedTimeSyncInfo.syncTime + m_monotonicClock.elapsed() - m_usedTimeSyncInfo.monotonicClockValue;
}

void TimeSynchronizationManager::onDbManagerInitialized()
{
    auto manager = m_connection->getMiscManager(Qn::kSystemAccess);

    ApiMiscData timePriorityData;
    const bool timePriorityStrLoadResult =
        manager->getMiscParamSync(
            LOCAL_TIME_PRIORITY_KEY_PARAM_NAME,
            &timePriorityData) == ErrorCode::ok;

    qint64 restoredTimeDelta = 0;
    TimePriorityKey restoredPriorityKey;
    const bool loadSyncTimeResult =
        loadSyncTime(
            m_connection,
            &restoredTimeDelta,
            &restoredPriorityKey);

    QnMutexLocker lock( &m_mutex );

    //restoring local time priority from DB
    if (timePriorityStrLoadResult)
    {
        const quint64 restoredPriorityKeyVal = timePriorityData.value.toULongLong();
        TimePriorityKey restoredPriorityKey;
        restoredPriorityKey.fromUInt64( restoredPriorityKeyVal );
        //considering time, restored from DB, to be unreliable
        restoredPriorityKey.flags &= ~Qn::TF_peerTimeSynchronizedWithInternetServer;

        if( m_localTimePriorityKey.sequence < restoredPriorityKey.sequence )
            m_localTimePriorityKey.sequence = restoredPriorityKey.sequence;
        if( restoredPriorityKey.flags & Qn::TF_peerTimeSetByUser )
            m_localTimePriorityKey.flags |= Qn::TF_peerTimeSetByUser;

        NX_LOGX( lit("Successfully restored time priority key 0x%1 from DB").arg(restoredPriorityKeyVal, 0, 16), cl_logWARNING );
    }

    boost::optional<std::pair<qint64, ec2::TimePriorityKey>> syncTimeDataToSave;
    if( m_timeSynchronized )
    {
        //saving time sync information to DB
        const qint64 curSyncTime = getSyncTimeNonSafe();
        syncTimeDataToSave = std::make_pair(
            QDateTime::currentMSecsSinceEpoch() - curSyncTime,
            m_usedTimeSyncInfo.timePriorityKey);
    }
    else
    {
        //restoring time sync information from DB
        if (loadSyncTimeResult)
        {
            m_usedTimeSyncInfo.syncTime = QDateTime::currentMSecsSinceEpoch() - restoredTimeDelta;
            m_usedTimeSyncInfo.timePriorityKey = restoredPriorityKey;
            m_usedTimeSyncInfo.timePriorityKey.flags &= ~Qn::TF_peerTimeSynchronizedWithInternetServer;
            // TODO: #ak The next line is doubtful. It may be needed in case if server was down
            // for a long time and time priority sequence has overflowed.
            m_usedTimeSyncInfo.timePriorityKey.flags &= ~Qn::TF_peerTimeSetByUser;
            NX_LOGX( lit("Successfully restored synchronized time %1 (delta %2, key 0x%3) from DB").
                arg(QDateTime::fromMSecsSinceEpoch(m_usedTimeSyncInfo.syncTime).toString(Qt::ISODate)).
                arg(restoredTimeDelta).
                arg(restoredPriorityKey.toUInt64(), 0, 16), cl_logINFO );
        }
    }

    auto localTimePriorityKey = m_localTimePriorityKey;
    lock.unlock();

    if (syncTimeDataToSave)
        saveSyncTimeSync(
            syncTimeDataToSave->first,
            syncTimeDataToSave->second);

    updateRuntimeInfoPriority( localTimePriorityKey.toUInt64() );
}

TimeSyncInfo TimeSynchronizationManager::getTimeSyncInfoNonSafe() const
{
    const qint64 elapsed = m_monotonicClock.elapsed();
    return TimeSyncInfo(
        elapsed,
        m_usedTimeSyncInfo.syncTime + elapsed - m_usedTimeSyncInfo.monotonicClockValue,
        m_usedTimeSyncInfo.timePriorityKey );
}

void TimeSynchronizationManager::syncTimeWithAllKnownServers(
    const QnMutexLockerBase& /*lock*/)
{
    for (std::pair<const QnUuid, PeerContext>& peerCtx : m_peersToSendTimeSyncTo)
    {
        NX_LOGX(lm("Scheduling time synchronization with peer %1")
            .arg(peerCtx.first.toString()), cl_logDEBUG2);
        m_timerManager->modifyTimerDelay(
            peerCtx.second.syncTimerID.get(),
            std::chrono::milliseconds::zero());
    }
}

void TimeSynchronizationManager::resyncTimeWithPeer(const QnUuid& peerId)
{
    QnMutexLocker lock(&m_mutex);
    auto peerCtx = m_peersToSendTimeSyncTo.find(peerId);
    if (peerCtx == m_peersToSendTimeSyncTo.end())
        return;
    NX_LOGX(lm("Scheduling time synchronization with peer %1")
        .arg(peerCtx->first.toString()), cl_logDEBUG2);
    m_timerManager->modifyTimerDelay(
        peerCtx->second.syncTimerID.get(),
        std::chrono::milliseconds::zero());
}

void TimeSynchronizationManager::onBeforeSendingTransaction(
    QnTransactionTransportBase* /*transport*/,
    nx_http::HttpHeaders* const headers)
{
    headers->emplace(
        QnTimeSyncRestHandler::TIME_SYNC_HEADER_NAME,
        getTimeSyncInfo().toString());
}

void TimeSynchronizationManager::onTransactionReceived(
    QnTransactionTransportBase* /*transport*/,
    const nx_http::HttpHeaders& headers)
{
    const auto& settings = m_messageBus->commonModule()->globalSettings();
    for (auto header : headers)
    {
        if (header.first != QnTimeSyncRestHandler::TIME_SYNC_HEADER_NAME)
            continue;

        const nx_http::StringType& serializedTimeSync = header.second;
        TimeSyncInfo remotePeerTimeSyncInfo;
        if (!remotePeerTimeSyncInfo.fromString(serializedTimeSync))
            continue;

        QnMutexLocker lock(&m_mutex);
        if (m_usedTimeSyncInfo.timePriorityKey.hasLessPriorityThan(
            remotePeerTimeSyncInfo.timePriorityKey,
            settings->isSynchronizingTimeWithInternet()))
        {
            syncTimeWithAllKnownServers(lock);
        }
        return;
    }
}

void TimeSynchronizationManager::forgetSynchronizedTimeNonSafe(
    QnMutexLockerBase* const lock)
{
    m_timeSynchronized = false;
    switchBackToLocalTime(lock);
}

void TimeSynchronizationManager::switchBackToLocalTime(QnMutexLockerBase* const lock)
{
    m_localSystemTimeDelta = std::numeric_limits<qint64>::min();
    ++m_localTimePriorityKey.sequence;
    m_localTimePriorityKey.flags &= ~Qn::TF_peerTimeSynchronizedWithInternetServer;
    m_usedTimeSyncInfo = TimeSyncInfo(
        m_monotonicClock.elapsed(),
        currentMSecsSinceEpoch(),
        m_localTimePriorityKey);
    handleLocalTimePriorityKeyChange(lock);
}

void TimeSynchronizationManager::checkSystemTimeForChange()
{
    using namespace std::chrono;

    {
        QnMutexLocker lock(&m_mutex);
        if (m_terminated)
            return;
    }
    const auto& settings = m_messageBus->commonModule()->globalSettings();
    const qint64 curSysTime = QDateTime::currentMSecsSinceEpoch();
    const int synchronizedToLocalTimeOffset = getSyncTime() - curSysTime;

    //local OS time has been changed. If system time is set
    //by local host time then updating system time
    const bool isSystemTimeSynchronizedWithInternet =
        settings->isSynchronizingTimeWithInternet() &&
        ((m_localTimePriorityKey.flags & Qn::TF_peerTimeSynchronizedWithInternetServer) > 0);

    const bool isTimeSynchronizedByThisPeerLocalTime =
        m_usedTimeSyncInfo.timePriorityKey == m_localTimePriorityKey &&
        !isSystemTimeSynchronizedWithInternet;

    const bool isSynchronizedToLocalTimeOffsetExceeded =
        qAbs(synchronizedToLocalTimeOffset) >
        duration_cast<milliseconds>(settings->maxDifferenceBetweenSynchronizedAndLocalTime()).count();

    bool isTimeChanged = false;
    if (isTimeSynchronizedByThisPeerLocalTime && isSynchronizedToLocalTimeOffsetExceeded)
    {
        NX_LOGX(lm("System time is synchronized with this peer's local time. "
            "Detected time shift %1 ms. Updating synchronized time...")
            .arg(synchronizedToLocalTimeOffset), cl_logDEBUG1);
        forceTimeResync();
        isTimeChanged = true;
    }
    else if (!isSystemTimeSynchronizedWithInternet)
    {
        // Checking whether it is appropriate to switch to local time.
        QnMutexLocker lk(&m_mutex);
        if (m_usedTimeSyncInfo.timePriorityKey.hasLessPriorityThan(
                m_localTimePriorityKey, settings->isSynchronizingTimeWithInternet()))
        {
            selectLocalTimeAsSynchronized(&lk, m_localTimePriorityKey.sequence);
            isTimeChanged = true;
        }
    }

    if (!isTimeChanged &&
        m_connection &&
        isSynchronizedToLocalTimeOffsetExceeded)
    {
        saveSyncTimeAsync(
            QDateTime::currentMSecsSinceEpoch() - getSyncTime(),
            m_usedTimeSyncInfo.timePriorityKey);
    }

    QnMutexLocker lock(&m_mutex);
    if (m_terminated)
        return;
    m_checkSystemTimeTaskID = m_timerManager->addTimer(
        std::bind(&TimeSynchronizationManager::checkSystemTimeForChange, this),
        std::chrono::milliseconds(SYSTEM_TIME_CHANGE_CHECK_PERIOD_MS));
}

void TimeSynchronizationManager::handleLocalTimePriorityKeyChange(
    QnMutexLockerBase* const lock)
{
    if (!m_connection)
        return;
#if 0
    ApiMiscData localTimeData(
        LOCAL_TIME_PRIORITY_KEY_PARAM_NAME,
        QByteArray::number(m_localTimePriorityKey.toUInt64()));

    auto manager = m_connection->getMiscManager(Qn::kSystemAccess);

    QnMutexUnlocker unlocker(lock);

    manager->saveMiscParam(localTimeData, this,
        [](int /*reqID*/, ec2::ErrorCode errCode)
        {
            if (errCode != ec2::ErrorCode::ok)
                qWarning() << "Failed to save time data to the database";
        });
#else
    // TODO: this is an old version from 3.0 We can switch to the new as soon as saveMiscParam will work asynchronously
    ++m_asyncOperationsInProgress;
    Ec2ThreadPool::instance()->start(make_custom_runnable(
        [this]
        {
            auto db = m_connection->messageBus()->getDb();
            QnTransaction<ApiMiscData> localTimeTran(
                ApiCommand::NotDefined,
                db->commonModule()->moduleGUID(),
                ec2::ApiMiscData(LOCAL_TIME_PRIORITY_KEY_PARAM_NAME,
                    QByteArray::number(m_localTimePriorityKey.toUInt64())));

            localTimeTran.transactionType = TransactionType::Local;
            db->transactionLog()->fillPersistentInfo(localTimeTran);
            db->executeTransaction(localTimeTran, QByteArray());

            --m_asyncOperationsInProgress;
            m_asyncOperationsWaitCondition.wakeOne();
        }));
#endif
}

void TimeSynchronizationManager::onTimeSynchronizationSettingsChanged()
{
    if (m_peerType != Qn::PeerType::PT_Server)
        return;
    const auto& settings = m_messageBus->commonModule()->globalSettings();
    if (settings->isSynchronizingTimeWithInternet())
    {
        QnMutexLocker lock(&m_mutex);
        if (m_internetSynchronizationTaskID > 0)
        {
            m_timerManager->modifyTimerDelay(
                m_internetSynchronizationTaskID, std::chrono::milliseconds::zero());
        }
        else
        {
            addInternetTimeSynchronizationTask();
        }
    }
    else if (m_usedTimeSyncInfo.timePriorityKey.isTakenFromInternet() ||
        m_localTimePriorityKey.isTakenFromInternet())
    {
        // Forgetting Internet time.
        QnMutexLocker lock(&m_mutex);
        syncTimeWithAllKnownServers(lock);
        switchBackToLocalTime(&lock);
    }
}

bool TimeSynchronizationManager::saveSyncTimeSync(
    qint64 syncTimeToLocalDelta,
    const TimePriorityKey& syncTimeKey)
{
    if (!m_connection)
        return false;

#if 0
    ApiMiscData deltaData(
        TIME_DELTA_PARAM_NAME,
        QByteArray::number(syncTimeToLocalDelta));

    ApiMiscData priorityData(
        USED_TIME_PRIORITY_KEY_PARAM_NAME,
        QByteArray::number(syncTimeKey.toUInt64()));

    auto manager = m_connection->getMiscManager(Qn::kSystemAccess);
    return
        manager->saveMiscParamSync(deltaData) == ErrorCode::ok &&
        manager->saveMiscParamSync(priorityData) == ErrorCode::ok;
#else
    // TODO: this is an old version from 3.0 We can switch to the new as soon as saveMiscParam will work asynchronously
    auto db = m_connection->messageBus()->getDb();
    QnTransaction<ApiMiscData> deltaTran(
        ApiCommand::NotDefined,
        db->commonModule()->moduleGUID(),
        ApiMiscData(TIME_DELTA_PARAM_NAME, QByteArray::number(syncTimeToLocalDelta)));

    QnTransaction<ApiMiscData> priorityTran(
        ApiCommand::NotDefined,
        db->commonModule()->moduleGUID(),
        ApiMiscData(USED_TIME_PRIORITY_KEY_PARAM_NAME,
            QByteArray::number(syncTimeKey.toUInt64())));

    deltaTran.transactionType = TransactionType::Local;
    priorityTran.transactionType = TransactionType::Local;

    db->transactionLog()->fillPersistentInfo(deltaTran);
    db->transactionLog()->fillPersistentInfo(priorityTran);

    bool result =
        db->executeTransaction(deltaTran, QByteArray()) == ErrorCode::ok &&
        db->executeTransaction(priorityTran, QByteArray()) == ErrorCode::ok;
    if (!result)
        NX_WARNING(this, lm("Can't save syncTime to the local DB."));
    return result;
#endif
}

void TimeSynchronizationManager::saveSyncTimeAsync(
    QnMutexLockerBase* const lock,
    qint64 syncTimeToLocalDelta,
    TimePriorityKey syncTimeKey)
{
    QnMutexUnlocker unlocker(lock);
    saveSyncTimeAsync(syncTimeToLocalDelta, syncTimeKey);
}

/**
* @param syncTimeToLocalDelta local_time - sync_time
*/
void TimeSynchronizationManager::saveSyncTimeAsync(
    qint64 syncTimeToLocalDelta,
    const TimePriorityKey& syncTimeKey)
{
    if (!m_connection)
        return;
#if 0
    ApiMiscData deltaData(
        TIME_DELTA_PARAM_NAME,
        QByteArray::number(syncTimeToLocalDelta));

    ApiMiscData priorityData(
        USED_TIME_PRIORITY_KEY_PARAM_NAME,
        QByteArray::number(syncTimeKey.toUInt64()));

    auto manager = m_connection->getMiscManager(Qn::kSystemAccess);
    manager->saveMiscParam(deltaData, this,
        [](int /*reqID*/, ErrorCode errCode)
    {
        if (errCode != ec2::ErrorCode::ok)
            NX_LOG(lm("Failed to save time data to the database"), cl_logWARNING);
    });
    manager->saveMiscParam(priorityData, this,
        [](int /*reqID*/, ErrorCode errCode)
    {
        if (errCode != ec2::ErrorCode::ok)
            NX_LOG(lm("Failed to save time data to the database"), cl_logWARNING);
    });
#else
    // TODO: this is an old version from 3.0 We can switch to the new as soon as saveMiscParam will work asynchronously
    ++m_asyncOperationsInProgress;
    Ec2ThreadPool::instance()->start(make_custom_runnable(
        [this, syncTimeToLocalDelta, syncTimeKey]()
        {
            saveSyncTimeSync(syncTimeToLocalDelta, syncTimeKey);

            --m_asyncOperationsInProgress;
             m_asyncOperationsWaitCondition.wakeOne();
        }));
#endif
}

} // namespace ec2
