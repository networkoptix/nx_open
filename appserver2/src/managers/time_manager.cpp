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


namespace ec2
{
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


    //////////////////////////////////////////////
    //   TimeSynchronizationManager
    //////////////////////////////////////////////
    static TimeSynchronizationManager* TimeManager_instance = nullptr;
    //static const size_t LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS = 10*60*1000;
    static const size_t LOCAL_SYSTEM_TIME_BROADCAST_PERIOD_MS = 10*1000;
    //!Once per 10 minutes checking if manual time server selection is required
    //static const size_t MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS = 10*60*1000;
    static const size_t MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS = 10*1000;
    static const size_t TIME_SYNCHRONIZATION_PERIOD_MS = 60*1000;

    //!Accurate time is fetched from internet with this period
    static const size_t INTERNET_SYNC_TIME_PERIOD_SEC = 600;
    //!If time synchronization with internet failes, period is multiplied on this value, but it cannot exceed \a MAX_PUBLIC_SYNC_TIME_PERIOD_SEC
    static const size_t INTERNET_SYNC_TIME_FAILURE_PERIOD_GROW_COEFF = 2;
    static const size_t MAX_INTERNET_SYNC_TIME_PERIOD_SEC = 24*60*60;
    static const size_t INTERNET_TIME_EXPIRATION_PERIOD_SEC = 7*24*60*60;   //one week
    static const size_t MILLIS_PER_SEC = 1000;
    //!Considering internet time equal to local time if difference is no more than this value
    static const qint64 MAX_LOCAL_SYSTEM_TIME_DRIFT = 10*1000;

    //TODO #ak save time to persistent storage and restore at startup

    TimeSynchronizationManager::TimeSynchronizationManager( Qn::PeerType peerType )
    :
        m_localSystemTimeDelta( std::numeric_limits<qint64>::min() ),
        m_timeDelta( 0 ),
        m_broadcastSysTimeTaskID( 0 ),
        m_internetSynchronizationTaskID( 0 ),
        m_manualTimerServerSelectionCheckTaskID( 0 ),
        m_terminated( false ),
        m_peerType( peerType ),
        m_internetTimeSynchronizationPeriod( INTERNET_SYNC_TIME_PERIOD_SEC )
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
        m_timeDelta = currentMSecsSinceEpoch();

        m_usedTimeSyncInfo = TimeSyncInfo(
            0,
            m_timeDelta,
            m_localTimePriorityKey ); 

        assert( TimeManager_instance == nullptr );
        TimeManager_instance = this;

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
        return m_monotonicClock.elapsed() + m_timeDelta; 
    }

    void TimeSynchronizationManager::primaryTimeServerChanged( const QnTransaction<ApiIdData>& tran )
    {
        QMutexLocker lk( &m_mutex );

        //received transaction

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
                m_timeDelta = currentMSecsSinceEpoch() - elapsed;
                m_usedTimeSyncInfo = TimeSyncInfo(
                    elapsed,
                    elapsed + m_timeDelta,
                    m_localTimePriorityKey ); 

                if( !synchronizingByCurrentServer )
                {
                    const qint64 curSyncTime = m_monotonicClock.elapsed() + m_timeDelta;
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
            elapsed + m_timeDelta,
            m_usedTimeSyncInfo.timePriorityKey );
    }

    qint64 TimeSynchronizationManager::getMonotonicClock() const
    {
        QMutexLocker lk( &m_mutex );
        return m_monotonicClock.elapsed();
    }

    void TimeSynchronizationManager::remotePeerTimeSyncUpdate(
        QMutexLocker* const lock,
        const QnId& /*remotePeerID*/,
        qint64 localMonotonicClock,
        qint64 remotePeerSyncTime,
        const TimePriorityKey& remotePeerTimePriorityKey )
    {
        assert( remotePeerTimePriorityKey.seed > 0 );

        //if there is new maximum remotePeerTimePriorityKey than updating delta and emitting timeChanged
        if( remotePeerTimePriorityKey > m_usedTimeSyncInfo.timePriorityKey )
        {
            //taking new synchronization data
            m_timeDelta = remotePeerSyncTime - localMonotonicClock;
            m_usedTimeSyncInfo = TimeSyncInfo(
                localMonotonicClock,
                remotePeerSyncTime,
                remotePeerTimePriorityKey ); 
            if( m_peerType == Qn::PT_Server )
            {
                using namespace std::placeholders;
                //sending broadcastPeerSystemTime tran, new sync time will be broadcasted along with it
                m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                    std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                    0 );
            }
            const qint64 curTime = m_monotonicClock.elapsed() + m_timeDelta;
            lock->unlock();
            emit timeChanged( curTime );
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

        QnTransaction<ApiPeerSystemTimeData> tran( ApiCommand::broadcastPeerSystemTime, false );
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
        std::multimap<unsigned int, std::map<QnId, TimeSyncInfo>::const_iterator, std::greater<unsigned int>> peersByTimePriorityFlags;
        for( auto it = m_systemTimeByPeer.cbegin(); it != m_systemTimeByPeer.cend(); ++it )
            peersByTimePriorityFlags.emplace( it->second.timePriorityKey.flags, it );

        if( peersByTimePriorityFlags.count(peersByTimePriorityFlags.cbegin()->first) > 1 )
        {
            const qint64 currentLocalTime = currentMSecsSinceEpoch();

            //list<pair<peerid, time> >
            QList<QPair<QnId, qint64> > peers;
            for( auto val: peersByTimePriorityFlags )
            {
                if( val.first != peersByTimePriorityFlags.cbegin()->first )
                    break;
                peers.push_back( QPair<QnId, qint64>( val.second->first, val.second->second.syncTime + (currentClock - val.second->second.monotonicClockValue) ) );
            }
            //multiple peers have same priority, user selection is required
            emit primaryTimeServerSelectionRequired( currentLocalTime, peers );
        }
    }

    void TimeSynchronizationManager::syncTimeWithInternet( quint64 /*taskID*/ )
    {
        {
            QMutexLocker lk( &m_mutex );

            m_internetSynchronizationTaskID = 0;
            if( m_terminated )
                return;
        }

        //synchronizing with some internet server
        using namespace std::placeholders;
        m_timeSynchronizer.getTimeAsync( std::bind( &TimeSynchronizationManager::onTimeFetchingDone, this, _1, _2 ) );
    }

    void TimeSynchronizationManager::onTimeFetchingDone( qint64 millisFromEpoch, SystemError::ErrorCode errorCode )
    {
        QMutexLocker lk( &m_mutex );

        if( millisFromEpoch > 0 )
        {
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

        using namespace std::placeholders;
        m_internetSynchronizationTaskID = TimerManager::instance()->addTimer(
            std::bind( &TimeSynchronizationManager::syncTimeWithInternet, this, _1 ),
            m_internetTimeSynchronizationPeriod * MILLIS_PER_SEC );
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

    void TimeSynchronizationManager::onPeerLost( ApiPeerAliveData data, bool /*isProxy*/ )
    {
        QMutexLocker lk( &m_mutex );
        m_systemTimeByPeer.erase( data.peer.id );
    }
}
