/**********************************************************
* 02 jul 2014
* a.kolesnikov
***********************************************************/

#include "time_manager.h"

#include <algorithm>

#include <QtConcurrent/QtConcurrent>
#include <QtCore/QDateTime>
#include <QtCore/QMutexLocker>

#include <common/common_module.h>
#include <transaction/transaction_message_bus.h>
#include <utils/common/joinable.h>
#include <utils/common/log.h>
#include <utils/common/timermanager.h>

#include "database/db_manager.h"
#include "ec2_thread_pool.h"


namespace ec2
{
    //!This parameter holds difference between local system time and synchronized time
    static const QByteArray TIME_DELTA_PARAM_NAME = "sync_time_delta";

    class SaveTimeDeltaTask
    :
        public QRunnable
    {
    public:
        SaveTimeDeltaTask( qint64 timeDelta )
        :
            m_timeDelta( timeDelta )
        {
            setAutoDelete( true );
        }

        virtual void run()
        {
            QnDbManager::instance()->saveMiscParam( TIME_DELTA_PARAM_NAME, QByteArray::number(m_timeDelta) );
        }

    private:
        qint64 m_timeDelta;
    };


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

    bool TimePriorityKey::operator<( const TimePriorityKey& right ) const
    {
        return
            sequence < right.sequence ? true :
            sequence > right.sequence ? false :
            flags < right.flags ? true :
            flags > right.flags ? false :
            seed < right.seed;
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
        sprintf( result.data(), "%lld,%lld,%lld", monotonicClockValue, syncTime, timePriorityKey.toUInt64() );
        result.resize( (int)strlen( result.data() ) );
        return result;
    }
    
    bool TimeSyncInfo::fromString( const QByteArray& str )
    {
        int curSepPos = 0;
        for( int i = 0; curSepPos < str.size(); ++i )
        {
            int nextSepPos = str.indexOf( ',', curSepPos );
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
    static TimeSynchronizationManager* TimeManager_instance = nullptr;
#ifdef TEST_PTS_SELECTION
    static const size_t LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS = 10 * 1000;
    static const size_t MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS = 10 * 1000;
#else
    static const size_t LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS = 10*60*1000;
    //!Once per 10 minutes checking if manual time server selection is required
    static const size_t MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS = 10 * 60 * 1000;
#endif

    //!Accurate time is fetched from internet with this period
    static const size_t INTERNET_SYNC_TIME_PERIOD_SEC = 600;
    //!If time synchronization with internet failes, period is multiplied on this value, but it cannot exceed \a MAX_PUBLIC_SYNC_TIME_PERIOD_SEC
    static const size_t INTERNET_SYNC_TIME_FAILURE_PERIOD_GROW_COEFF = 2;
    static const size_t MAX_INTERNET_SYNC_TIME_PERIOD_SEC = 24*60*60;
    static const size_t INTERNET_TIME_EXPIRATION_PERIOD_SEC = 7*24*60*60;   //one week
    static const size_t MILLIS_PER_SEC = 1000;
    //!Considering internet time equal to local time if difference is no more than this value
    static const qint64 MAX_LOCAL_SYSTEM_TIME_DRIFT = 10*1000;

    TimeSynchronizationManager::TimeSynchronizationManager( Qn::PeerType peerType )
    :
        m_localSystemTimeDelta( std::numeric_limits<qint64>::min() ),
        m_broadcastSysTimeTaskID( 0 ),
        m_internetSynchronizationTaskID( 0 ),
        m_manualTimerServerSelectionCheckTaskID( 0 ),
        m_terminated( false ),
        m_peerType( peerType ),
        m_internetTimeSynchronizationPeriod( INTERNET_SYNC_TIME_PERIOD_SEC ),
        m_timeSynchronized( false )
    {
        if( peerType == Qn::PT_Server )
            m_localTimePriorityKey.flags |= peerIsServer;

#ifndef EDGE_SERVER
        m_localTimePriorityKey.flags |= peerIsNotEdgeServer;
#endif
        m_localTimePriorityKey.seed = (rand() | (rand() * RAND_MAX)) + 1;
        //TODO #ak handle priority key duplicates or use guid?
        if( QElapsedTimer::isMonotonic() )
            m_localTimePriorityKey.flags |= peerHasMonotonicClock;

        m_monotonicClock.restart();
        //initializing synchronized time with local system time
        m_usedTimeSyncInfo = TimeSyncInfo(
            0,
            currentMSecsSinceEpoch(),
            m_localTimePriorityKey ); 

        assert( TimeManager_instance == nullptr );
        TimeManager_instance = this;

        connect( QnDbManager::instance(), &QnDbManager::initialized, 
                 this, &TimeSynchronizationManager::onDbManagerInitialized,
                 Qt::DirectConnection );
        connect( QnTransactionMessageBus::instance(), &QnTransactionMessageBus::newDirectConnectionEstablished,
                 this, &TimeSynchronizationManager::onNewConnectionEstablished,
                 Qt::DirectConnection );
        connect( QnTransactionMessageBus::instance(), &QnTransactionMessageBus::peerLost,
                 this, &TimeSynchronizationManager::onPeerLost,
                 Qt::DirectConnection );

        using namespace std::placeholders;
        if( peerType == Qn::PT_Server )
        {
            m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                0 );
            m_internetSynchronizationTaskID = TimerManager::instance()->addTimer(
                std::bind( &TimeSynchronizationManager::syncTimeWithInternet, this, _1 ),
                0 );
            NX_LOG( lit( "TimeSynchronizationManager. Added time sync task %1, delay %2" ).arg( m_internetSynchronizationTaskID ).arg( m_internetTimeSynchronizationPeriod * MILLIS_PER_SEC ), cl_logDEBUG2 );
        }
        else
            m_manualTimerServerSelectionCheckTaskID = TimerManager::instance()->addTimer(
                std::bind( &TimeSynchronizationManager::checkIfManualTimeServerSelectionIsRequired, this, _1 ),
                MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS );
    }

    TimeSynchronizationManager::~TimeSynchronizationManager()
    {
        quint64 broadcastSysTimeTaskID = 0;
        quint64 manualTimerServerSelectionCheckTaskID = 0;
        quint64 internetSynchronizationTaskID = 0;
        {
            QMutexLocker lk( &m_mutex );
            m_terminated = false;
            broadcastSysTimeTaskID = m_broadcastSysTimeTaskID;
            manualTimerServerSelectionCheckTaskID = m_manualTimerServerSelectionCheckTaskID;
            internetSynchronizationTaskID = m_internetSynchronizationTaskID;
        }

        if( broadcastSysTimeTaskID )
        {
            TimerManager::instance()->joinAndDeleteTimer( m_broadcastSysTimeTaskID );
            m_broadcastSysTimeTaskID = 0;
        }

        if( manualTimerServerSelectionCheckTaskID )
        {
            TimerManager::instance()->joinAndDeleteTimer( manualTimerServerSelectionCheckTaskID );
            m_manualTimerServerSelectionCheckTaskID = 0;
        }

        if( internetSynchronizationTaskID )
        {
            TimerManager::instance()->joinAndDeleteTimer( internetSynchronizationTaskID );
            m_internetSynchronizationTaskID = 0;
        }

        assert( TimeManager_instance == this );
        TimeManager_instance = nullptr;

        m_timeSynchronizer.pleaseStop();
        m_timeSynchronizer.join();
    }

    TimeSynchronizationManager* TimeSynchronizationManager::instance()
    {
        return TimeManager_instance;
    }

    qint64 TimeSynchronizationManager::getSyncTime() const
    {
        QMutexLocker lk( &m_mutex );
        return m_usedTimeSyncInfo.syncTime + m_monotonicClock.elapsed() - m_usedTimeSyncInfo.monotonicClockValue; 
    }

    void TimeSynchronizationManager::primaryTimeServerChanged( const QnTransaction<ApiIdData>& tran )
    {
        QMutexLocker lk( &m_mutex );

        NX_LOG( lit("Received primary time server change transaction. new peer %1, local peer %2").
            arg( tran.params.id.toString() ).arg( qnCommon->moduleGUID().toString() ), cl_logDEBUG1 );

        if( tran.params.id == qnCommon->moduleGUID() )
        {
            //local peer is selected by user as primary time server
            const bool synchronizingByCurrentServer = m_usedTimeSyncInfo.timePriorityKey == m_localTimePriorityKey;
            m_localTimePriorityKey.flags |= peerTimeSetByUser;
            //incrementing sequence 
            m_localTimePriorityKey.sequence = m_usedTimeSyncInfo.timePriorityKey.sequence + 1;
            if( m_localTimePriorityKey > m_usedTimeSyncInfo.timePriorityKey )
            {
                //using current server time info
                const qint64 elapsed = m_monotonicClock.elapsed();
                //selection of peer as primary time server means it's local system time is to be used as synchronized time
                m_usedTimeSyncInfo = TimeSyncInfo(
                    elapsed,
                    currentMSecsSinceEpoch(),
                    m_localTimePriorityKey );
                NX_LOG( lit("Received primary time server change transaction. New synchronized time %1, new priority key 0x%2").
                    arg(QDateTime::fromMSecsSinceEpoch(m_usedTimeSyncInfo.syncTime).toString(Qt::ISODate)).arg(m_localTimePriorityKey.toUInt64(), 0, 16), cl_logWARNING );
                m_timeSynchronized = true;
                //saving synchronized time to DB
                if( QnDbManager::instance() && QnDbManager::instance()->isInitialized() )
                    Ec2ThreadPool::instance()->start( new SaveTimeDeltaTask( 0 ) );

                if( !synchronizingByCurrentServer )
                {
                    const qint64 curSyncTime = m_usedTimeSyncInfo.syncTime;
                    using namespace std::placeholders;
                    //sending broadcastPeerSystemTime tran, new sync time will be broadcasted along with it
                    m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                        std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                        0 );
                    lk.unlock();
                    emit timeChanged( curSyncTime );
                }
            }
        }
        else
        {
            m_localTimePriorityKey.flags &= ~peerTimeSetByUser;
        }
    }

    void TimeSynchronizationManager::peerSystemTimeReceived( const QnTransaction<ApiPeerSystemTimeData>& tran )
    {
        QMutexLocker lk( &m_mutex );

        NX_LOG( lit("Received peer %1 system time (%2), peer time priority key 0x%3").
            arg( tran.params.peerID.toString() ).arg( QDateTime::fromMSecsSinceEpoch( tran.params.peerSysTime ).toString( Qt::ISODate ) ).
            arg(tran.params.timePriorityKey, 0, 16), cl_logDEBUG2 );

        TimePriorityKey peerPriorityKey;
        peerPriorityKey.fromUInt64( tran.params.timePriorityKey );
        m_systemTimeByPeer[tran.params.peerID] = TimeSyncInfo(
            m_monotonicClock.elapsed(),
            tran.params.peerSysTime,
            peerPriorityKey );
    }

    TimeSyncInfo TimeSynchronizationManager::getTimeSyncInfo() const
    {
        QMutexLocker lk( &m_mutex );

        const qint64 elapsed = m_monotonicClock.elapsed();
        return TimeSyncInfo(
            elapsed,
            m_usedTimeSyncInfo.syncTime + elapsed - m_usedTimeSyncInfo.monotonicClockValue,
            m_usedTimeSyncInfo.timePriorityKey );
    }

    qint64 TimeSynchronizationManager::getMonotonicClock() const
    {
        QMutexLocker lk( &m_mutex );
        return m_monotonicClock.elapsed();
    }

    void TimeSynchronizationManager::remotePeerTimeSyncUpdate(
        QMutexLocker* const lock,
        const QUuid& remotePeerID,
        qint64 localMonotonicClock,
        qint64 remotePeerSyncTime,
        const TimePriorityKey& remotePeerTimePriorityKey )
    {
        assert( remotePeerTimePriorityKey.seed > 0 );

        NX_LOG( lit("Received sync time update from peer %1, peer's sync time (%2), peer's time priority key 0x%3. Local peer id %4, used priority key 0x%5").
            arg(remotePeerID.toString()).arg(QDateTime::fromMSecsSinceEpoch(remotePeerSyncTime).toString(Qt::ISODate)).
            arg( remotePeerTimePriorityKey.toUInt64(), 0, 16 ).arg( qnCommon->moduleGUID().toString() ).
            arg(m_usedTimeSyncInfo.timePriorityKey.toUInt64(), 0, 16), cl_logDEBUG2 );

        //if there is new maximum remotePeerTimePriorityKey than updating delta and emitting timeChanged
        if( remotePeerTimePriorityKey > m_usedTimeSyncInfo.timePriorityKey )
        {
            NX_LOG( lit("Received sync time update from peer %1, peer's sync time (%2), peer's time priority key 0x%3. Local peer id %4, used priority key 0x%5. Accepting peer's synchronized time").
                arg( remotePeerID.toString() ).arg( QDateTime::fromMSecsSinceEpoch( remotePeerSyncTime ).toString( Qt::ISODate ) ).
                arg( remotePeerTimePriorityKey.toUInt64(), 0, 16 ).arg( qnCommon->moduleGUID().toString() ).
                arg(m_usedTimeSyncInfo.timePriorityKey.toUInt64(), 0, 16), cl_logWARNING );
            //taking new synchronization data
            m_usedTimeSyncInfo = TimeSyncInfo(
                localMonotonicClock,
                remotePeerSyncTime,
                remotePeerTimePriorityKey ); 
            const qint64 curSyncTime = m_usedTimeSyncInfo.syncTime + m_monotonicClock.elapsed() - m_usedTimeSyncInfo.monotonicClockValue;
            //saving synchronized time to DB
            m_timeSynchronized = true;
            if( QnDbManager::instance() && QnDbManager::instance()->isInitialized() )
                Ec2ThreadPool::instance()->start( new SaveTimeDeltaTask( QDateTime::currentMSecsSinceEpoch() - curSyncTime ) );
            if( m_peerType == Qn::PT_Server )
            {
                using namespace std::placeholders;
                //sending broadcastPeerSystemTime tran, new sync time will be broadcasted along with it
                m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                    std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                    0 );
            }
            lock->unlock();
            emit timeChanged( curSyncTime );
            lock->relock();
        }
    }

    void TimeSynchronizationManager::onNewConnectionEstablished( const QnTransactionTransportPtr& transport )
    {
        using namespace std::placeholders;
        transport->getSocket()->toggleStatisticsCollection( true );
        transport->setBeforeSendingChunkHandler( std::bind( &TimeSynchronizationManager::onBeforeSendingHttpChunk, this, _1, _2 ) );
        transport->setHttpChunkExtensonHandler( std::bind( &TimeSynchronizationManager::onRecevingHttpChunkExtensions, this, _1, _2 ) );
    }

    //!used to pass time sync information between peers
    static const QByteArray TIME_SYNC_EXTENSION_NAME( "time_sync" );

    void TimeSynchronizationManager::onBeforeSendingHttpChunk(
        QnTransactionTransport* /*transport*/,
        std::vector<nx_http::ChunkExtension>* const extensions )
    {
        //synchronizing time periodically with every transaction, why not?
        extensions->emplace_back( TIME_SYNC_EXTENSION_NAME, getTimeSyncInfo().toString() );
    }

    void TimeSynchronizationManager::broadcastLocalSystemTime( quint64 taskID )
    {
        {
            QMutexLocker lk( &m_mutex );
            if( m_terminated )
            {
                m_broadcastSysTimeTaskID = 0;
                return;
            }
            if( taskID != m_broadcastSysTimeTaskID )
                return;

            using namespace std::placeholders;
            m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS );
        }

        //TODO #ak if local time changes have to broadcast it as soon as possible

        NX_LOG( lit("Broadcasting local system time. peer %1, system time time (%2), local time priority key 0x%3").
            arg( qnCommon->moduleGUID().toString() ).arg( QDateTime::fromMSecsSinceEpoch( currentMSecsSinceEpoch() ).toString( Qt::ISODate ) ).
            arg(m_localTimePriorityKey.toUInt64(), 0, 16), cl_logDEBUG2 );

        QnTransaction<ApiPeerSystemTimeData> tran( ApiCommand::broadcastPeerSystemTime );
        tran.params.peerID = qnCommon->moduleGUID();
        tran.params.timePriorityKey = m_localTimePriorityKey.toUInt64();
        {
            QMutexLocker lk( &m_mutex );
            tran.params.peerSysTime = currentMSecsSinceEpoch();
        }
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

        if( m_systemTimeByPeer.empty() )
            return;

        const qint64 currentClock = m_monotonicClock.elapsed();
        //map<priority flags, m_systemTimeByPeer iterator>
        std::multimap<unsigned int, std::map<QUuid, TimeSyncInfo>::const_iterator, std::greater<unsigned int>> peersByTimePriorityFlags;
        for( auto it = m_systemTimeByPeer.cbegin(); it != m_systemTimeByPeer.cend(); ++it )
            peersByTimePriorityFlags.emplace( it->second.timePriorityKey.flags, it );

#ifndef TEST_PTS_SELECTION
        if( peersByTimePriorityFlags.count(peersByTimePriorityFlags.cbegin()->first) > 1 )
#endif
        {
            const qint64 currentLocalTime = currentMSecsSinceEpoch();

            //list<pair<peerid, time> >
            QList<QPair<QUuid, qint64> > peers;
            for( auto val: peersByTimePriorityFlags )
            {
                if( val.first != peersByTimePriorityFlags.cbegin()->first )
                    break;
                peers.push_back( QPair<QUuid, qint64>( val.second->first, val.second->second.syncTime + (currentClock - val.second->second.monotonicClockValue) ) );
            }
            //multiple peers have same priority, user selection is required
            emit primaryTimeServerSelectionRequired( currentLocalTime, peers );
        }
    }

    void TimeSynchronizationManager::syncTimeWithInternet( quint64 taskID )
    {
        NX_LOG( lit( "TimeSynchronizationManager::syncTimeWithInternet. taskID %1" ).arg( taskID ), cl_logDEBUG2 );

        {
            QMutexLocker lk( &m_mutex );

            m_internetSynchronizationTaskID = 0;
            if( m_terminated )
                return;
        }

        NX_LOG( lit("Synchronizing time with internet"), cl_logDEBUG1 );

        //synchronizing with some internet server
        using namespace std::placeholders;
        m_timeSynchronizer.getTimeAsync( std::bind( &TimeSynchronizationManager::onTimeFetchingDone, this, _1, _2 ) );
    }

    void TimeSynchronizationManager::onTimeFetchingDone( qint64 millisFromEpoch, SystemError::ErrorCode errorCode )
    {
        QMutexLocker lk( &m_mutex );

        if( millisFromEpoch > 0 )
        {
            NX_LOG( lit("Received time %1 from the internet").arg(QDateTime::fromMSecsSinceEpoch(millisFromEpoch).toString(Qt::ISODate)), cl_logDEBUG1 );

            m_internetTimeSynchronizationPeriod = INTERNET_SYNC_TIME_PERIOD_SEC;

            const qint64 curLocalTime = currentMSecsSinceEpoch();

            //using received time
            const auto flagsBak = m_localTimePriorityKey.flags;
            m_localTimePriorityKey.flags |= peerTimeSynchronizedWithInternetServer;

            if( (llabs(curLocalTime - millisFromEpoch) > MAX_LOCAL_SYSTEM_TIME_DRIFT) || (flagsBak != m_localTimePriorityKey.flags) )
            {
                //sending broadcastPeerSystemTime tran, new sync time will be broadcasted along with it
                m_localSystemTimeDelta = millisFromEpoch - m_monotonicClock.elapsed();

                remotePeerTimeSyncUpdate(
                    &lk,
                    qnCommon->moduleGUID(),
                    m_monotonicClock.elapsed(),
                    millisFromEpoch,
                    m_localTimePriorityKey );

                using namespace std::placeholders;
                m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                    std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                    0 );
            }
        }
        else
        {
            NX_LOG( lit("Failed to get time from the internet. %1").arg(SystemError::toString(errorCode)), cl_logDEBUG1 );
            //failure
            m_internetTimeSynchronizationPeriod = std::min<>(
                m_internetTimeSynchronizationPeriod * INTERNET_SYNC_TIME_FAILURE_PERIOD_GROW_COEFF,
                MAX_INTERNET_SYNC_TIME_PERIOD_SEC );
        }

        assert( m_internetSynchronizationTaskID == 0 );

        using namespace std::placeholders;
        m_internetSynchronizationTaskID = TimerManager::instance()->addTimer(
            std::bind( &TimeSynchronizationManager::syncTimeWithInternet, this, _1 ),
            m_internetTimeSynchronizationPeriod * MILLIS_PER_SEC );
        NX_LOG( lit( "TimeSynchronizationManager. Added time sync task %1, delay %2" ).arg( m_internetSynchronizationTaskID ).arg( m_internetTimeSynchronizationPeriod * MILLIS_PER_SEC ), cl_logDEBUG2 );
    }

    qint64 TimeSynchronizationManager::currentMSecsSinceEpoch() const
    {
        return m_localSystemTimeDelta == std::numeric_limits<qint64>::min()
            ? QDateTime::currentMSecsSinceEpoch()
            : m_monotonicClock.elapsed() + m_localSystemTimeDelta;
    }

    void TimeSynchronizationManager::onRecevingHttpChunkExtensions(
        QnTransactionTransport* transport,
        const std::vector<nx_http::ChunkExtension>& extensions )
    {
        for( auto extension: extensions )
        {
            if( extension.first == TIME_SYNC_EXTENSION_NAME )
            {
                //taking into account tcp connection round trip time
                unsigned int rttMillis = 0;
                StreamSocketInfo sockInfo;
                if( transport->getSocket()->getConnectionStatistics( &sockInfo ) )
                    rttMillis = sockInfo.rttVar;

                TimeSyncInfo remotePeerTimeSyncInfo;
                if( !remotePeerTimeSyncInfo.fromString( extension.second ) )
                    continue;
        
                QMutexLocker lk( &m_mutex );
                remotePeerTimeSyncUpdate(
                    &lk,
                    transport->remotePeer().id,
                    m_monotonicClock.elapsed(),
                    remotePeerTimeSyncInfo.syncTime + rttMillis / 2,
                    remotePeerTimeSyncInfo.timePriorityKey );
                return;
            }
        }
    }

    void TimeSynchronizationManager::onPeerLost( ApiPeerAliveData data )
    {
        QMutexLocker lk( &m_mutex );
        m_systemTimeByPeer.erase( data.peer.id );
    }

    void TimeSynchronizationManager::onDbManagerInitialized()
    {
        QMutexLocker lk( &m_mutex );

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
                NX_LOG( lit("Successfully restored synchronized time %1 (delta %2) from DB").
                    arg(QDateTime::fromMSecsSinceEpoch(m_usedTimeSyncInfo.syncTime).toString(Qt::ISODate)).arg(timeDeltaStr.toLongLong()), cl_logWARNING );
            }
        }
    }
}
