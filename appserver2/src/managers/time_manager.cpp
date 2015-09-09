/**********************************************************
* 02 jul 2014
* a.kolesnikov
***********************************************************/

#include "time_manager.h"

#include <algorithm>
#include <atomic>

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QDateTime>
#include <QtCore/QMutexLocker>
#ifdef Q_OS_MACX
#include <zlib.h>
#else
#include <QtZlib/zlib.h>
#endif

#include <api/runtime_info_manager.h>

#include <common/common_module.h>
#include <http/custom_headers.h>
#include <transaction/transaction_message_bus.h>

#include <nx_ec/data/api_runtime_data.h>

#include <utils/common/joinable.h>
#include <utils/common/log.h>
#include <utils/common/timermanager.h>
#include <utils/network/time/time_protocol_client.h>
#include <utils/network/time/multiple_internet_time_fetcher.h>

#include "database/db_manager.h"
#include "ec2_thread_pool.h"
#include "rest/time_sync_rest_handler.h"


namespace ec2
{
    //!This parameter holds difference between local system time and synchronized time
    static const QByteArray TIME_DELTA_PARAM_NAME = "sync_time_delta";
    static const QByteArray TIME_PRIORITY_KEY_PARAM_NAME = "time_priority_key";

    template<class Function>
    class CustomRunnable
    :
        public QRunnable
    {
    public:
        CustomRunnable( Function&& function )
        :
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


    //////////////////////////////////////////////
    //   TimePriorityKey
    //////////////////////////////////////////////
    TimePriorityKey::TimePriorityKey()
    :
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

    bool TimePriorityKey::operator<( const TimePriorityKey& right ) const
    {
        const int peerIsServerSet = flags & TimeSynchronizationManager::peerIsServer;
        const int right_peerIsServerSet = right.flags & TimeSynchronizationManager::peerIsServer;
        if( peerIsServerSet < right_peerIsServerSet )
            return true;
        if( right_peerIsServerSet < peerIsServerSet )
            return false;

        const int internetFlagSet = flags & TimeSynchronizationManager::peerTimeSynchronizedWithInternetServer;
        const int right_internetFlagSet = right.flags & TimeSynchronizationManager::peerTimeSynchronizedWithInternetServer;
        if( internetFlagSet < right_internetFlagSet )
            return true;
        if( right_internetFlagSet < internetFlagSet )
            return false;

        return
            sequence < right.sequence ? true :
            sequence > right.sequence ? false :
            flags < right.flags ? true :
            flags > right.flags ? false :
            seed < right.seed;
    }
    
    bool TimePriorityKey::operator<=( const TimePriorityKey& right ) const
    {
        return !(*this > right);
    }

    bool TimePriorityKey::operator>( const TimePriorityKey& right ) const
    {
        return right < *this;
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
    static const size_t INITIAL_INTERNET_SYNC_TIME_PERIOD_SEC = 0;
    static const size_t MIN_INTERNET_SYNC_TIME_PERIOD_SEC = 60;
    static const char* RFC868_SERVERS[] = { "time.nist.gov", "time.ien.it"/*, "time1.ucla.edu"*/ };
#ifdef _DEBUG
    static const size_t LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS = 10*MILLIS_PER_SEC;
    static const size_t MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS = 60*MILLIS_PER_SEC;
    static const size_t INTERNET_SYNC_TIME_PERIOD_SEC = 60;
    //!Reporting time synchronization information to other peers once per this period
    static const int TIME_SYNC_SEND_TIMEOUT_SEC = 10;
#else
    static const size_t LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS = 10*60*MILLIS_PER_SEC;
    //!Once per 10 minutes checking if manual time server selection is required
    static const size_t MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS = 10*60*MILLIS_PER_SEC;
    //!Accurate time is fetched from internet with this period
    static const size_t INTERNET_SYNC_TIME_PERIOD_SEC = 24*60*60;
    //!Reporting time synchronization information to other peers once per this period
    static const int TIME_SYNC_SEND_TIMEOUT_SEC = 10*60;
#endif
    //!If time synchronization with internet failes, period is multiplied on this value, but it cannot exceed \a MAX_PUBLIC_SYNC_TIME_PERIOD_SEC
    static const size_t INTERNET_SYNC_TIME_FAILURE_PERIOD_GROW_COEFF = 2;
    static const size_t MAX_INTERNET_SYNC_TIME_PERIOD_SEC = 7*24*60*60;
    //static const size_t INTERNET_TIME_EXPIRATION_PERIOD_SEC = 7*24*60*60;   //one week
    //!Considering internet time equal to local time if difference is no more than this value
    static const qint64 MAX_LOCAL_SYSTEM_TIME_DRIFT_MS = 10*MILLIS_PER_SEC;
    //!Maximum allowed drift between synchronized time and time received via internet
    static const qint64 MAX_SYNC_VS_INTERNET_TIME_DRIFT_MS = 20*MILLIS_PER_SEC;
    static const int MAX_SEQUENT_INTERNET_SYNCHRONIZATION_FAILURES = 15;
    static const int MAX_RTT_TIME_MS = 10 * 1000;
    //!Maximum time drift between servers that we want to keep up to
    static const int MAX_DESIRED_TIME_DRIFT_MS = 1000;
    //!Considering that other server's time is retrieved with error not less then \a MIN_GET_TIME_ERROR_MS
    /*!
        This should help against redundant clock resync
    */
    static const int MIN_GET_TIME_ERROR_MS = 100;

    static_assert( MIN_INTERNET_SYNC_TIME_PERIOD_SEC > 0, "MIN_INTERNET_SYNC_TIME_PERIOD_SEC MUST be > 0!" );
    static_assert( MIN_INTERNET_SYNC_TIME_PERIOD_SEC <= MAX_INTERNET_SYNC_TIME_PERIOD_SEC,
        "Check MIN_INTERNET_SYNC_TIME_PERIOD_SEC and MAX_INTERNET_SYNC_TIME_PERIOD_SEC" );
    static_assert( INTERNET_SYNC_TIME_PERIOD_SEC >= MIN_INTERNET_SYNC_TIME_PERIOD_SEC,
        "Check INTERNET_SYNC_TIME_PERIOD_SEC and MIN_INTERNET_SYNC_TIME_PERIOD_SEC" );
    static_assert( INTERNET_SYNC_TIME_PERIOD_SEC <= MAX_INTERNET_SYNC_TIME_PERIOD_SEC,
        "Check INTERNET_SYNC_TIME_PERIOD_SEC and MAX_INTERNET_SYNC_TIME_PERIOD_SEC" );

    TimeSynchronizationManager::TimeSynchronizationManager( Qn::PeerType peerType )
    :
        m_localSystemTimeDelta( std::numeric_limits<qint64>::min() ),
        m_broadcastSysTimeTaskID( 0 ),
        m_internetSynchronizationTaskID( 0 ),
        m_manualTimerServerSelectionCheckTaskID( 0 ),
        m_terminated( false ),
        m_peerType( peerType ),
        m_internetTimeSynchronizationPeriod( INITIAL_INTERNET_SYNC_TIME_PERIOD_SEC ),
        m_timeSynchronized( false ),
        m_internetSynchronizationFailureCount( 0 )
    {
    }

    TimeSynchronizationManager::~TimeSynchronizationManager()
    {
        pleaseStop();
    }

    void TimeSynchronizationManager::pleaseStop()
    {
        quint64 broadcastSysTimeTaskID = 0;
        quint64 manualTimerServerSelectionCheckTaskID = 0;
        quint64 internetSynchronizationTaskID = 0;
        {
            QMutexLocker lk( &m_mutex );
            m_terminated = false;
            broadcastSysTimeTaskID = m_broadcastSysTimeTaskID;
            m_broadcastSysTimeTaskID = 0;
            manualTimerServerSelectionCheckTaskID = m_manualTimerServerSelectionCheckTaskID;
            m_manualTimerServerSelectionCheckTaskID = 0;
            internetSynchronizationTaskID = m_internetSynchronizationTaskID;
            m_internetSynchronizationTaskID = 0;
        }

        if( broadcastSysTimeTaskID )
            TimerManager::instance()->joinAndDeleteTimer( broadcastSysTimeTaskID );

        if( manualTimerServerSelectionCheckTaskID )
            TimerManager::instance()->joinAndDeleteTimer( manualTimerServerSelectionCheckTaskID );

        if( internetSynchronizationTaskID )
            TimerManager::instance()->joinAndDeleteTimer( internetSynchronizationTaskID );

        if( m_timeSynchronizer )
        {
            m_timeSynchronizer->pleaseStop();
            m_timeSynchronizer->join();
            m_timeSynchronizer.reset();
        }
    }

    void TimeSynchronizationManager::start()
    {
        if( m_peerType == Qn::PT_Server )
            m_localTimePriorityKey.flags |= peerIsServer;

#ifndef EDGE_SERVER
        m_localTimePriorityKey.flags |= peerIsNotEdgeServer;
#endif
        QByteArray localGUID = qnCommon->moduleGUID().toByteArray();
        m_localTimePriorityKey.seed = crc32(0, (const Bytef*)localGUID.constData(), localGUID.size());
        //TODO #ak use guid to avoid handle priority key duplicates
        if( QElapsedTimer::isMonotonic() )
            m_localTimePriorityKey.flags |= peerHasMonotonicClock;

        m_monotonicClock.restart();
        //initializing synchronized time with local system time
        m_usedTimeSyncInfo = TimeSyncInfo(
            0,
            currentMSecsSinceEpoch(),
            m_localTimePriorityKey );

        if (QnDbManager::instance())
            connect( QnDbManager::instance(), &QnDbManager::initialized, 
                 this, &TimeSynchronizationManager::onDbManagerInitialized,
                 Qt::DirectConnection );
        connect( QnTransactionMessageBus::instance(), &QnTransactionMessageBus::newDirectConnectionEstablished,
                 this, &TimeSynchronizationManager::onNewConnectionEstablished,
                 Qt::DirectConnection );
        connect( QnTransactionMessageBus::instance(), &QnTransactionMessageBus::peerLost,
                 this, &TimeSynchronizationManager::onPeerLost,
                 Qt::DirectConnection );

        {
            QMutexLocker lk( &m_mutex );

            using namespace std::placeholders;
            if( m_peerType == Qn::PT_Server )
            {
                m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                    std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                    0 );
                std::unique_ptr<MultipleInternetTimeFetcher> multiFetcher( new MultipleInternetTimeFetcher() );

                for(const char* timeServer: RFC868_SERVERS)
                    multiFetcher->addTimeFetcher(std::unique_ptr<AbstractAccurateTimeFetcher>(
                        new TimeProtocolClient(QLatin1String(timeServer))));
                m_timeSynchronizer = std::move( multiFetcher );
                addInternetTimeSynchronizationTask();
            }
            else
                m_manualTimerServerSelectionCheckTaskID = TimerManager::instance()->addTimer(
                    std::bind( &TimeSynchronizationManager::checkIfManualTimeServerSelectionIsRequired, this, _1 ),
                    MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS );
        }
    }

    qint64 TimeSynchronizationManager::getSyncTime() const
    {
        QMutexLocker lk( &m_mutex );
        return getSyncTimeNonSafe();
    }

    void TimeSynchronizationManager::primaryTimeServerChanged( const QnTransaction<ApiIdData>& tran )
    {
        quint64 localTimePriorityBak = 0;
        quint64 newLocalTimePriority = 0;
        {
            QMutexLocker lk( &m_mutex );

            localTimePriorityBak = m_localTimePriorityKey.toUInt64();

            NX_LOG( lit("TimeSynchronizationManager. Received primary time server change transaction. new peer %1, local peer %2").
                arg( tran.params.id.toString() ).arg( qnCommon->moduleGUID().toString() ), cl_logDEBUG1 );

            if( tran.params.id == qnCommon->moduleGUID() )
            {
                //local peer is selected by user as primary time server
                const bool synchronizingByCurrentServer = m_usedTimeSyncInfo.timePriorityKey == m_localTimePriorityKey;
                m_localTimePriorityKey.flags |= peerTimeSetByUser;
                //incrementing sequence 
                m_localTimePriorityKey.sequence = m_usedTimeSyncInfo.timePriorityKey.sequence + 1;
                //"select primary time server" means "take its local time", so resetting internet synchronization flag
                m_localTimePriorityKey.flags &= ~peerTimeSynchronizedWithInternetServer;
                if( m_localTimePriorityKey > m_usedTimeSyncInfo.timePriorityKey )
                {
                    //using current server time info
                    const qint64 elapsed = m_monotonicClock.elapsed();
                    //selection of peer as primary time server means it's local system time is to be used as synchronized time 
                        //in case of internet connection absence
                    m_usedTimeSyncInfo = TimeSyncInfo(
                        elapsed,
                        currentMSecsSinceEpoch(),
                        m_localTimePriorityKey );
                    //resetting "synchronized with internet" flag
                    NX_LOG( lit("TimeSynchronizationManager. Received primary time server change transaction. New synchronized time %1, new priority key 0x%2").
                        arg(QDateTime::fromMSecsSinceEpoch(m_usedTimeSyncInfo.syncTime).toString(Qt::ISODate)).arg(m_localTimePriorityKey.toUInt64(), 0, 16), cl_logINFO );
                    m_timeSynchronized = true;
                    //saving synchronized time to DB
                    if( QnDbManager::instance() && QnDbManager::instance()->isInitialized() )
                        Ec2ThreadPool::instance()->start( make_custom_runnable( std::bind(
                            &QnDbManager::saveMiscParam,
                            QnDbManager::instance(),
                            TIME_DELTA_PARAM_NAME,
                            QByteArray::number(0) ) ) );

                    if( !synchronizingByCurrentServer )
                    {
                        const qint64 curSyncTime = m_usedTimeSyncInfo.syncTime;
                        using namespace std::placeholders;
                        //sending broadcastPeerSystemTime tran, new sync time will be broadcasted along with it
                        if( !m_terminated )
                        {
                            if( m_broadcastSysTimeTaskID )
                                TimerManager::instance()->deleteTimer( m_broadcastSysTimeTaskID );
                            m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                                std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                                0 );
                        }
                        lk.unlock();
                        WhileExecutingDirectCall callGuard( this );
                        emit timeChanged( curSyncTime );
                    }

                    //reporting new sync time to everyone we know
                    syncTimeWithAllKnownServers(&lk);
                }
            }
            else
            {
                m_localTimePriorityKey.flags &= ~peerTimeSetByUser;
            }
            newLocalTimePriority = m_localTimePriorityKey.toUInt64();
            if( newLocalTimePriority != localTimePriorityBak && QnDbManager::instance() && QnDbManager::instance()->isInitialized() )
                Ec2ThreadPool::instance()->start( make_custom_runnable( std::bind(
                    &QnDbManager::saveMiscParam,
                    QnDbManager::instance(),
                    TIME_PRIORITY_KEY_PARAM_NAME,
                    QByteArray::number(newLocalTimePriority) ) ) );
        }

        /* Can cause signal, going out of mutex locker. */
        if( newLocalTimePriority != localTimePriorityBak )
            updateRuntimeInfoPriority(newLocalTimePriority);
    }

    void TimeSynchronizationManager::peerSystemTimeReceived( const QnTransaction<ApiPeerSystemTimeData>& tran )
    {
        QMutexLocker lk( &m_mutex );

        peerSystemTimeReceivedNonSafe( tran.params );

        lk.unlock();
        WhileExecutingDirectCall callGuard( this );
        emit peerTimeChanged( tran.params.peerID, getSyncTime(), tran.params.peerSysTime );
    }

    void TimeSynchronizationManager::knownPeersSystemTimeReceived( const QnTransaction<ApiPeerSystemTimeDataList>& tran )
    {
        for( const ApiPeerSystemTimeData& data: tran.params )
        {
            {
                QMutexLocker lk( &m_mutex );
                peerSystemTimeReceivedNonSafe( data );
            }
            WhileExecutingDirectCall callGuard( this );
            emit peerTimeChanged( data.peerID, getSyncTime(), data.peerSysTime );
        }
    }

    TimeSyncInfo TimeSynchronizationManager::getTimeSyncInfo() const
    {
        QMutexLocker lk( &m_mutex );
        return getTimeSyncInfoNonSafe();
    }

    qint64 TimeSynchronizationManager::getMonotonicClock() const
    {
        QMutexLocker lk( &m_mutex );
        return m_monotonicClock.elapsed();
    }

    void TimeSynchronizationManager::forgetSynchronizedTime()
    {
        QMutexLocker lk( &m_mutex );

        m_localSystemTimeDelta = std::numeric_limits<qint64>::min();
        m_systemTimeByPeer.clear();
        m_timeSynchronized = false;
        m_localTimePriorityKey.flags &= ~peerTimeSynchronizedWithInternetServer;
        m_usedTimeSyncInfo = TimeSyncInfo(
            m_monotonicClock.elapsed(),
            currentMSecsSinceEpoch(),
            m_localTimePriorityKey );
    }

    QnPeerTimeInfoList TimeSynchronizationManager::getPeerTimeInfoList() const
    {
        QMutexLocker lk( &m_mutex );

        //list<pair<peerid, time> >
        QnPeerTimeInfoList peers;

        const qint64 currentClock = m_monotonicClock.elapsed();
        for( auto it = m_systemTimeByPeer.cbegin(); it != m_systemTimeByPeer.cend(); ++it )
            peers.push_back( QnPeerTimeInfo( it->first, it->second.syncTime + (currentClock - it->second.monotonicClockValue) ) );

        return peers;
    }

    ApiPeerSystemTimeDataList TimeSynchronizationManager::getKnownPeersSystemTime() const
    {
        QMutexLocker lk( &m_mutex );

        ApiPeerSystemTimeDataList result;
        result.reserve( m_systemTimeByPeer.size() );
        const qint64 currentClock = m_monotonicClock.elapsed();
        for( auto it = m_systemTimeByPeer.cbegin(); it != m_systemTimeByPeer.cend(); ++it )
        {
            ApiPeerSystemTimeData data;
            data.peerID = it->first;
            data.timePriorityKey = it->second.timePriorityKey.toUInt64();
            data.peerSysTime = it->second.syncTime + (currentClock - it->second.monotonicClockValue);
            result.push_back( std::move(data) );
        }

        return result;
    }

    void TimeSynchronizationManager::processTimeSyncInfoHeader(
        const QnUuid& peerID,
        const nx_http::StringType& serializedTimeSync,
        AbstractStreamSocket* sock )
    {
        //taking into account tcp connection round trip time
        unsigned int rttMillis = 0;
        StreamSocketInfo sockInfo;
        if( sock && sock->getConnectionStatistics( &sockInfo ) )
            rttMillis = sockInfo.rttVar;

        TimeSyncInfo remotePeerTimeSyncInfo;
        if( !remotePeerTimeSyncInfo.fromString( serializedTimeSync ) )
            return;

        Q_ASSERT( remotePeerTimeSyncInfo.timePriorityKey.seed > 0 );

        //TODO #ak following condition should be removed. Placed for debug purpose only
        if( rttMillis > MAX_RTT_TIME_MS )
        {
            NX_LOG( lit( "%1. Received rtt of %2 ms" ).arg( Q_FUNC_INFO ).arg( rttMillis ), cl_logWARNING );
            rttMillis = MAX_DESIRED_TIME_DRIFT_MS;
        }

        QMutexLocker lk( &m_mutex );
        remotePeerTimeSyncUpdate(
            &lk,
            peerID,
            m_monotonicClock.elapsed(),
            remotePeerTimeSyncInfo.syncTime + rttMillis / 2,
            remotePeerTimeSyncInfo.timePriorityKey,
            rttMillis );
    }

    void TimeSynchronizationManager::remotePeerTimeSyncUpdate(
        QMutexLocker* const lock,
        const QnUuid& remotePeerID,
        qint64 localMonotonicClock,
        qint64 remotePeerSyncTime,
        const TimePriorityKey& remotePeerTimePriorityKey,
        const qint64 timeErrorEstimation )
    {
        Q_ASSERT( remotePeerTimePriorityKey.seed > 0 );

        NX_LOG( QString::fromLatin1("TimeSynchronizationManager. Received sync time update from peer %1, "
            "peer's sync time (%2), peer's time priority key 0x%3. Local peer id %4, used priority key 0x%5").
            arg(remotePeerID.toString()).arg(QDateTime::fromMSecsSinceEpoch(remotePeerSyncTime).toString(Qt::ISODate)).
            arg(remotePeerTimePriorityKey.toUInt64(), 0, 16).arg(qnCommon->moduleGUID().toString()).
            arg(m_usedTimeSyncInfo.timePriorityKey.toUInt64(), 0, 16), cl_logDEBUG2 );

        //time difference between this server and remote one is not that great
        const auto timeDifference = remotePeerSyncTime - getSyncTimeNonSafe();
        const bool maxTimeDriftExceeded =
            (std::abs( timeDifference ) >= std::max<qint64>( timeErrorEstimation * 2, MIN_GET_TIME_ERROR_MS ));
        const bool needAdjustClockDueToLargeDrift =
            //taking drift into account if both servers have time with same key
            (remotePeerTimePriorityKey == m_usedTimeSyncInfo.timePriorityKey) &&
            maxTimeDriftExceeded &&
            (remotePeerID > qnCommon->moduleGUID());

        //if there is new maximum remotePeerTimePriorityKey then updating delta and emitting timeChanged
        if( (remotePeerTimePriorityKey <= m_usedTimeSyncInfo.timePriorityKey) &&
            !needAdjustClockDueToLargeDrift )
        {
            return; //not applying time
        }

        NX_LOG( QString::fromLatin1("TimeSynchronizationManager. Received sync time update from peer %1, peer's sync time (%2), "
            "peer's time priority key 0x%3. Local peer id %4, used priority key 0x%5. Accepting peer's synchronized time").
            arg(remotePeerID.toString()).arg(QDateTime::fromMSecsSinceEpoch(remotePeerSyncTime).toString(Qt::ISODate)).
            arg(remotePeerTimePriorityKey.toUInt64(), 0, 16).arg(qnCommon->moduleGUID().toString()).
            arg(m_usedTimeSyncInfo.timePriorityKey.toUInt64(), 0, 16), cl_logINFO );
        //taking new synchronization data
        m_usedTimeSyncInfo = TimeSyncInfo(
            localMonotonicClock,
            remotePeerSyncTime,
            remotePeerTimePriorityKey ); 
        if( needAdjustClockDueToLargeDrift )
            ++m_usedTimeSyncInfo.timePriorityKey.sequence;   //for case if synchronizing because of time drift
        const qint64 curSyncTime = m_usedTimeSyncInfo.syncTime + m_monotonicClock.elapsed() - m_usedTimeSyncInfo.monotonicClockValue;
        //saving synchronized time to DB
        m_timeSynchronized = true;
        if( QnDbManager::instance() && QnDbManager::instance()->isInitialized() )
            Ec2ThreadPool::instance()->start( make_custom_runnable( std::bind(
                &QnDbManager::saveMiscParam,
                QnDbManager::instance(),
                TIME_DELTA_PARAM_NAME,
                QByteArray::number(QDateTime::currentMSecsSinceEpoch() - curSyncTime) ) ) );
        if( m_peerType == Qn::PT_Server )
        {
            using namespace std::placeholders;
            //sending broadcastPeerSystemTime tran, new sync time will be broadcasted along with it
            if( !m_terminated )
            {
                if( m_broadcastSysTimeTaskID )
                    TimerManager::instance()->deleteTimer( m_broadcastSysTimeTaskID );
                m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                    std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                    0 );
            }
        }
        lock->unlock();
        {
            WhileExecutingDirectCall callGuard( this );
            emit timeChanged( curSyncTime );
        }
        lock->relock();

        //informing all servers we can about time change as soon as possible
        syncTimeWithAllKnownServers(lock);
    }

    void TimeSynchronizationManager::onNewConnectionEstablished( QnTransactionTransport* transport )
    {
        using namespace std::placeholders;

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

            //we can connect to the peer
            const QUrl remoteAddr = transport->remoteAddr();
            //saving credentials has been used to establish connection
            startSynchronizingTimeWithPeer( //starting sending time sync info to peer
                transport->remotePeer().id,
                SocketAddress( remoteAddr.host(), remoteAddr.port() ),
                transport->authData() );

            using namespace std::placeholders;
        }
    }

    void TimeSynchronizationManager::onPeerLost( ApiPeerAliveData data )
    {
        stopSynchronizingTimeWithPeer( data.peer.id );

        QMutexLocker lk( &m_mutex );
        m_systemTimeByPeer.erase( data.peer.id );
    }

    void TimeSynchronizationManager::startSynchronizingTimeWithPeer(
        const QnUuid& peerID,
        SocketAddress peerAddress,
        nx_http::AuthInfoCache::AuthorizationCacheItem authData )
    {
        QMutexLocker lk( &m_mutex );
        auto iterResultPair = m_peersToSendTimeSyncTo.emplace(
            peerID,
            PeerContext( std::move(peerAddress), std::move(authData) ) );
        if( !iterResultPair.second )
            return; //already exists
        PeerContext& ctx = iterResultPair.first->second;
        //adding periodic task
        ctx.syncTimerID = TimerManager::instance()->addTimer(
            std::bind(&TimeSynchronizationManager::synchronizeWithPeer, this, peerID),
            0 );    //performing initial synchronization immediately
    }

    void TimeSynchronizationManager::stopSynchronizingTimeWithPeer( const QnUuid& peerID )
    {
        TimerManager::TimerGuard timerID;
        nx_http::AsyncHttpClientPtr httpClient;

        QMutexLocker lk( &m_mutex );
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
        nx_http::AsyncHttpClientPtr clientPtr;

        QMutexLocker lk( &m_mutex );
        if( m_terminated )
            return;
        auto peerIter = m_peersToSendTimeSyncTo.find( peerID );
        if( peerIter == m_peersToSendTimeSyncTo.end() )
            return;
        QUrl targetUrl;
        targetUrl.setScheme( lit("http") );
        targetUrl.setHost( peerIter->second.peerAddress.address.toString() );
        targetUrl.setPort( peerIter->second.peerAddress.port );
        targetUrl.setPath( lit("/") + QnTimeSyncRestHandler::PATH );

        Q_ASSERT( !peerIter->second.httpClient );
        clientPtr = nx_http::AsyncHttpClient::create();
        using namespace std::placeholders;
        connect(
            clientPtr.get(), &nx_http::AsyncHttpClient::done,
            this,
            [this, peerID]( nx_http::AsyncHttpClientPtr clientPtr ) {
                timeSyncRequestDone(peerID, std::move(clientPtr));
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
        clientPtr->addAdditionalHeader( Qn::PEER_GUID_HEADER_NAME, qnCommon->moduleGUID().toByteArray() );

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

        if( !clientPtr->doGet( targetUrl ) )
        {
            clientPtr.reset();
            peerIter->second.syncTimerID = TimerManager::instance()->addTimer(
                std::bind( &TimeSynchronizationManager::synchronizeWithPeer, this, peerID ),
                TIME_SYNC_SEND_TIMEOUT_SEC * MILLIS_PER_SEC );
        }
        else
        {
            peerIter->second.syncTimerID.reset();
        }
        peerIter->second.httpClient.swap( clientPtr );
        //clientPtr will be destroyed after mutex unlock
    }

    void TimeSynchronizationManager::timeSyncRequestDone(
        const QnUuid& peerID,
        nx_http::AsyncHttpClientPtr clientPtr )
    {
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
                    sock.data() );
            }
        }

        QMutexLocker lk( &m_mutex );
        auto peerIter = m_peersToSendTimeSyncTo.find( peerID );
        if( peerIter == m_peersToSendTimeSyncTo.end() )
            return;
        Q_ASSERT( !peerIter->second.syncTimerID );
        peerIter->second.httpClient.reset();
        //scheduling next synchronization
        if( m_terminated )
            return;
        peerIter->second.syncTimerID = TimerManager::instance()->addTimer(
            std::bind( &TimeSynchronizationManager::synchronizeWithPeer, this, peerID ),
            TIME_SYNC_SEND_TIMEOUT_SEC * MILLIS_PER_SEC );
    }

    void TimeSynchronizationManager::broadcastLocalSystemTime( quint64 taskID )
    {
        {
            QMutexLocker lk( &m_mutex );
            if( (taskID != m_broadcastSysTimeTaskID) || m_terminated )
                return;

            using namespace std::placeholders;
            m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS );
        }

        //TODO #ak if local time changes have to broadcast it as soon as possible

        NX_LOG( lit("TimeSynchronizationManager. Broadcasting local system time. peer %1, system time time (%2), local time priority key 0x%3").
            arg( qnCommon->moduleGUID().toString() ).arg( QDateTime::fromMSecsSinceEpoch( currentMSecsSinceEpoch() ).toString( Qt::ISODate ) ).
            arg(m_localTimePriorityKey.toUInt64(), 0, 16), cl_logDEBUG2 );

        QnTransaction<ApiPeerSystemTimeData> tran( ApiCommand::broadcastPeerSystemTime );
        tran.params.peerID = qnCommon->moduleGUID();
        tran.params.timePriorityKey = m_localTimePriorityKey.toUInt64();
        {
            QMutexLocker lk( &m_mutex );
            tran.params.peerSysTime = QDateTime::currentMSecsSinceEpoch();  //currentMSecsSinceEpoch();
        }
        peerSystemTimeReceived( tran ); //remembering own system time
        QnTransactionMessageBus::instance()->sendTransaction( tran );
    }

    void TimeSynchronizationManager::checkIfManualTimeServerSelectionIsRequired( quint64 /*taskID*/ )
    {
        //TODO #ak it is better to run this method on event, not by timeout
        QMutexLocker lk( &m_mutex );

        m_manualTimerServerSelectionCheckTaskID = 0;
        if( m_terminated )
            return;

        using namespace std::placeholders;
        m_manualTimerServerSelectionCheckTaskID = TimerManager::instance()->addTimer(
            std::bind( &TimeSynchronizationManager::checkIfManualTimeServerSelectionIsRequired, this, _1 ),
            MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS );

        if( m_systemTimeByPeer.empty() ||
            (m_usedTimeSyncInfo.timePriorityKey.flags & peerTimeSynchronizedWithInternetServer) > 0 ||
            (m_localTimePriorityKey.flags & peerTimeSynchronizedWithInternetServer) > 0 )
        {
            //we know nothing about other peers. 
            //Or we have time taken from the Internet, which means someone has connection to the Internet, 
                //so no sense to ask user to select primary time server
            return;
        }

        //map<priority flags, m_systemTimeByPeer iterator>
        std::multimap<unsigned int, std::map<QnUuid, TimeSyncInfo>::const_iterator, std::greater<unsigned int>> peersByTimePriorityFlags;
        for( auto it = m_systemTimeByPeer.cbegin(); it != m_systemTimeByPeer.cend(); ++it )
            peersByTimePriorityFlags.emplace( it->second.timePriorityKey.flags, it );

#ifndef TEST_PTS_SELECTION
        if( (peersByTimePriorityFlags.count(peersByTimePriorityFlags.cbegin()->first) > 1) &&               //multiple servers have same priority
            ((peersByTimePriorityFlags.cbegin()->first & peerTimeSynchronizedWithInternetServer) == 0) )    //those servers do not have internet access
#endif
        {
            WhileExecutingDirectCall callGuard( this );
            //multiple peers have same priority, user selection is required
            emit primaryTimeServerSelectionRequired();
        }
    }

    void TimeSynchronizationManager::syncTimeWithInternet( quint64 taskID )
    {
        NX_LOG( lit( "TimeSynchronizationManager. TimeSynchronizationManager::syncTimeWithInternet. taskID %1" ).arg( taskID ), cl_logDEBUG2 );

        QMutexLocker lk( &m_mutex );

        if( (taskID != m_internetSynchronizationTaskID) || m_terminated )
            return;
        m_internetSynchronizationTaskID = 0;

        NX_LOG( lit("TimeSynchronizationManager. Synchronizing time with internet"), cl_logDEBUG1 );

        //synchronizing with some internet server
        using namespace std::placeholders;
        if( !m_timeSynchronizer->getTimeAsync( std::bind( &TimeSynchronizationManager::onTimeFetchingDone, this, _1, _2 ) ) )
        {
            NX_LOG( lit( "TimeSynchronizationManager. Failed to start internet time synchronization. %1" ).arg( SystemError::getLastOSErrorText() ), cl_logDEBUG1 );
            //failure
            m_internetTimeSynchronizationPeriod = std::min<>(
                MIN_INTERNET_SYNC_TIME_PERIOD_SEC + m_internetTimeSynchronizationPeriod * INTERNET_SYNC_TIME_FAILURE_PERIOD_GROW_COEFF,
                MAX_INTERNET_SYNC_TIME_PERIOD_SEC );

            addInternetTimeSynchronizationTask();
        }
    }

    void TimeSynchronizationManager::onTimeFetchingDone( const qint64 millisFromEpoch, SystemError::ErrorCode errorCode )
    {
        quint64 localTimePriorityBak = 0;
        quint64 newLocalTimePriority = 0;
        {
            QMutexLocker lk( &m_mutex );

            localTimePriorityBak = m_localTimePriorityKey.toUInt64();

            if( millisFromEpoch > 0 )
            {
                NX_LOG( lit("TimeSynchronizationManager. Received time %1 from the internet").
                    arg(QDateTime::fromMSecsSinceEpoch(millisFromEpoch).toString(Qt::ISODate)), cl_logDEBUG1 );

                m_internetSynchronizationFailureCount = 0;

                m_internetTimeSynchronizationPeriod = INTERNET_SYNC_TIME_PERIOD_SEC;

                const qint64 curLocalTime = currentMSecsSinceEpoch();

                //using received time
                const auto localTimePriorityKeyBak = m_localTimePriorityKey;
                m_localTimePriorityKey.flags |= peerTimeSynchronizedWithInternetServer;

                if( llabs(getSyncTimeNonSafe() - millisFromEpoch) > MAX_SYNC_VS_INTERNET_TIME_DRIFT_MS )
                {
                    //considering time synchronized time as inaccurate, even if it is marked as received from internet
                    m_localTimePriorityKey.sequence = m_usedTimeSyncInfo.timePriorityKey.sequence + 1;
                }

                if( (llabs(curLocalTime - millisFromEpoch) > MAX_LOCAL_SYSTEM_TIME_DRIFT_MS) ||
                    (localTimePriorityKeyBak != m_localTimePriorityKey) )
                {
                    //sending broadcastPeerSystemTime tran, new sync time will be broadcasted along with it
                    m_localSystemTimeDelta = millisFromEpoch - m_monotonicClock.elapsed();

                    remotePeerTimeSyncUpdate(
                        &lk,
                        qnCommon->moduleGUID(),
                        m_monotonicClock.elapsed(),
                        millisFromEpoch,
                        m_localTimePriorityKey,
                        MAX_SYNC_VS_INTERNET_TIME_DRIFT_MS );

                    if( !m_terminated )
                    {
                        if( m_broadcastSysTimeTaskID )
                            TimerManager::instance()->deleteTimer( m_broadcastSysTimeTaskID );
                        using namespace std::placeholders;
                        m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                            std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                            0 );
                    }
                }
            }
            else
            {
                NX_LOG( lit("TimeSynchronizationManager. Failed to get time from the internet. %1").arg(SystemError::toString(errorCode)), cl_logDEBUG1 );
                //failure
                m_internetTimeSynchronizationPeriod = std::min<>(
                    MIN_INTERNET_SYNC_TIME_PERIOD_SEC + m_internetTimeSynchronizationPeriod * INTERNET_SYNC_TIME_FAILURE_PERIOD_GROW_COEFF,
                    MAX_INTERNET_SYNC_TIME_PERIOD_SEC );

                ++m_internetSynchronizationFailureCount;
                if( m_internetSynchronizationFailureCount > MAX_SEQUENT_INTERNET_SYNCHRONIZATION_FAILURES )
                    m_localTimePriorityKey.flags &= ~peerTimeSynchronizedWithInternetServer;
            }

            addInternetTimeSynchronizationTask();

            newLocalTimePriority = m_localTimePriorityKey.toUInt64();
            if( newLocalTimePriority != localTimePriorityBak && QnDbManager::instance() && QnDbManager::instance()->isInitialized() )
                Ec2ThreadPool::instance()->start( make_custom_runnable( std::bind(
                    &QnDbManager::saveMiscParam,
                    QnDbManager::instance(),
                    TIME_PRIORITY_KEY_PARAM_NAME,
                    QByteArray::number(newLocalTimePriority) ) ) );
        }

        /* Can cause signal, going out of mutex locker. */
        if( newLocalTimePriority != localTimePriorityBak )
            updateRuntimeInfoPriority(newLocalTimePriority);
    }

    void TimeSynchronizationManager::addInternetTimeSynchronizationTask()
    {
        Q_ASSERT( m_internetSynchronizationTaskID == 0 );

        if( m_terminated )
            return;

        using namespace std::placeholders;
        m_internetSynchronizationTaskID = TimerManager::instance()->addTimer(
            std::bind( &TimeSynchronizationManager::syncTimeWithInternet, this, _1 ),
            m_internetTimeSynchronizationPeriod * MILLIS_PER_SEC );
        NX_LOG( lit( "TimeSynchronizationManager. Added internet time sync task %1, delay %2" ).
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
        QnPeerRuntimeInfo localInfo = QnRuntimeInfoManager::instance()->localInfo();
        if (localInfo.data.peer.peerType != Qn::PT_Server)
            return;

        if (localInfo.data.serverTimePriority == priority)
            return;

        localInfo.data.serverTimePriority = priority;
        QnRuntimeInfoManager::instance()->updateLocalItem(localInfo);
    }

    qint64 TimeSynchronizationManager::getSyncTimeNonSafe() const
    {
        return m_usedTimeSyncInfo.syncTime + m_monotonicClock.elapsed() - m_usedTimeSyncInfo.monotonicClockValue; 
    }

    void TimeSynchronizationManager::onDbManagerInitialized()
    {
        QMutexLocker lk( &m_mutex );

        //restoring local time priority from DB
        QByteArray timePriorityStr;
        if( QnDbManager::instance()->readMiscParam( TIME_PRIORITY_KEY_PARAM_NAME, &timePriorityStr ) )
        {
            const quint64 restoredPriorityKeyVal = timePriorityStr.toULongLong();
            TimePriorityKey restoredPriorityKey;
            restoredPriorityKey.fromUInt64( restoredPriorityKeyVal );
            //considering time, restored from DB, to be unreliable
            restoredPriorityKey.flags &= ~peerTimeSynchronizedWithInternetServer;

            if( m_localTimePriorityKey.sequence < restoredPriorityKey.sequence )
                m_localTimePriorityKey.sequence = restoredPriorityKey.sequence;
            if( restoredPriorityKey.flags & peerTimeSetByUser )
                m_localTimePriorityKey.flags |= peerTimeSetByUser;

            ApiPeerSystemTimeData peerSystemTimeData;
            peerSystemTimeData.peerID = qnCommon->moduleGUID();
            peerSystemTimeData.timePriorityKey = m_localTimePriorityKey.toUInt64();
            peerSystemTimeData.peerSysTime = QDateTime::currentMSecsSinceEpoch();
            peerSystemTimeReceivedNonSafe( peerSystemTimeData );

            NX_LOG( lit("TimeSynchronizationManager. Successfully restored time priority key %1 from DB").arg(restoredPriorityKeyVal), cl_logWARNING );
        }

        if( m_timeSynchronized )
        {
            //saving time sync information to DB
            const qint64 curSyncTime = m_usedTimeSyncInfo.syncTime + m_monotonicClock.elapsed() - m_usedTimeSyncInfo.monotonicClockValue;
            QnDbManager::instance()->saveMiscParam( TIME_DELTA_PARAM_NAME, QByteArray::number(QDateTime::currentMSecsSinceEpoch() - curSyncTime) );
        }
        else
        {
            //restoring time sync information from DB
            QByteArray timeDeltaStr;
            if( QnDbManager::instance()->readMiscParam( TIME_DELTA_PARAM_NAME, &timeDeltaStr ) )
            {
                m_usedTimeSyncInfo.syncTime = QDateTime::currentMSecsSinceEpoch() - timeDeltaStr.toLongLong();
                NX_LOG( lit("TimeSynchronizationManager. Successfully restored synchronized time %1 (delta %2) from DB").
                    arg(QDateTime::fromMSecsSinceEpoch(m_usedTimeSyncInfo.syncTime).toString(Qt::ISODate)).arg(timeDeltaStr.toLongLong()), cl_logINFO );
            }
        }

        auto localTimePriorityKey = m_localTimePriorityKey;
        lk.unlock();
        updateRuntimeInfoPriority( localTimePriorityKey.toUInt64() );
    }

    void TimeSynchronizationManager::peerSystemTimeReceivedNonSafe( const ApiPeerSystemTimeData& data )
    {
        NX_LOG( lit("TimeSynchronizationManager. Received peer %1 system time (%2), peer time priority key 0x%3").
            arg(data.peerID.toString()).arg( QDateTime::fromMSecsSinceEpoch(data.peerSysTime).toString( Qt::ISODate ) ).
            arg(data.timePriorityKey, 0, 16), cl_logDEBUG2 );

        TimePriorityKey peerPriorityKey;
        peerPriorityKey.fromUInt64( data.timePriorityKey );
        m_systemTimeByPeer[data.peerID] = TimeSyncInfo(
            m_monotonicClock.elapsed(),
            data.peerSysTime,
            peerPriorityKey );
    }

    TimeSyncInfo TimeSynchronizationManager::getTimeSyncInfoNonSafe() const
    {
        const qint64 elapsed = m_monotonicClock.elapsed();
        return TimeSyncInfo(
            elapsed,
            m_usedTimeSyncInfo.syncTime + elapsed - m_usedTimeSyncInfo.monotonicClockValue,
            m_usedTimeSyncInfo.timePriorityKey );
    }

    void TimeSynchronizationManager::syncTimeWithAllKnownServers(QMutexLocker* const /*lock*/)
    {
        for (std::pair<const QnUuid, PeerContext>& peerCtx : m_peersToSendTimeSyncTo)
        {
            TimerManager::instance()->modifyTimerDelay(
                peerCtx.second.syncTimerID.get(),
                0);
        }
    }

    void TimeSynchronizationManager::onBeforeSendingTransaction(
        QnTransactionTransport* transport,
        nx_http::HttpHeaders* const headers)
    {
        headers->emplace(
            QnTimeSyncRestHandler::TIME_SYNC_HEADER_NAME,
            getTimeSyncInfo().toString());
    }

    void TimeSynchronizationManager::onTransactionReceived(
        QnTransactionTransport* transport,
        const nx_http::HttpHeaders& headers)
    {
        for (auto header : headers)
        {
            if (header.first != QnTimeSyncRestHandler::TIME_SYNC_HEADER_NAME)
                continue;

            const nx_http::StringType& serializedTimeSync = header.second;
            TimeSyncInfo remotePeerTimeSyncInfo;
            if (!remotePeerTimeSyncInfo.fromString(serializedTimeSync))
                continue;

            QMutexLocker lk(&m_mutex);
            if (remotePeerTimeSyncInfo.timePriorityKey > m_usedTimeSyncInfo.timePriorityKey)
                syncTimeWithAllKnownServers(&lk);
            return;
        }
    }
}
