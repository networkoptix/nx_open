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
#include <utils/common/timermanager.h>


namespace ec2
{
    //////////////////////////////////////////////
    //   TimePriorityKey
    //////////////////////////////////////////////
    TimePriorityKey::TimePriorityKey()
    :
        flags(0),
        sequence(0),
        seed(0)
    {
    }

    bool TimePriorityKey::operator==( const TimePriorityKey& right ) const
    {
        return flags == right.flags &&
               sequence == right.sequence &&
               seed == right.seed;
    }

    bool TimePriorityKey::operator<( const TimePriorityKey& right ) const
    {
        return flags < right.flags ? true :
               flags > right.flags ? false :
               sequence < right.sequence ? true :
               sequence > right.sequence ? false :
               seed < right.seed;
    }
    
    bool TimePriorityKey::operator>( const TimePriorityKey& right ) const
    {
        return right < *this;
    }

    quint64 TimePriorityKey::toUInt64() const
    {
        return ((quint64)flags << 48) | ((quint64)sequence << 32) | seed;
    }
    
    void TimePriorityKey::fromUInt64( quint64 val )
    {
        flags = (quint16)(val >> 48);
        sequence = (quint16)((val >> 32) & 0xFFFF00000000LL);
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
        QByteArray result;
        result.resize( 20*3 + 1 );
        sprintf( result.data(), "%lld,%lld,%lld", monotonicClockValue, syncTime, timePriorityKey.toUInt64() );
        result.resize( strlen( result.data() ) );
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

    TimeSynchronizationManager::TimeSynchronizationManager( Qn::PeerType peerType )
    :
        m_timeDelta( 0 ),
        m_broadcastSysTimeTaskID( 0 ),
        m_manualTimerServerSelectionCheckTaskID( 0 ),
        m_terminated( false )
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
        m_timeDelta = QDateTime::currentMSecsSinceEpoch();

        m_usedTimeSyncInfo = TimeSyncInfo(
            0,
            m_timeDelta,
            m_localTimePriorityKey ); 

        assert( TimeManager_instance == nullptr );
        TimeManager_instance = this;

        connect( QnTransactionMessageBus::instance(), &QnTransactionMessageBus::newConnectionEstablished,
                 this, &TimeSynchronizationManager::onNewConnectionEstablished,
                 Qt::DirectConnection );

        using namespace std::placeholders;
        if( peerType == Qn::PT_Server )
            m_broadcastSysTimeTaskID = TimerManager::instance()->addTimer(
                std::bind( &TimeSynchronizationManager::broadcastLocalSystemTime, this, _1 ),
                0 );
        else
            m_manualTimerServerSelectionCheckTaskID = TimerManager::instance()->addTimer(
                std::bind( &TimeSynchronizationManager::checkIfManualTimeServerSelectionIsRequired, this, _1 ),
                MANUAL_TIME_SERVER_SELECTION_NECESSITY_CHECK_PERIOD_MS );
    }

    TimeSynchronizationManager::~TimeSynchronizationManager()
    {
        quint64 broadcastSysTimeTaskID = 0;
        quint64 manualTimerServerSelectionCheckTaskID = 0;
        {
            QMutexLocker lk( &m_mutex );
            m_terminated = false;
            broadcastSysTimeTaskID = m_broadcastSysTimeTaskID;
            manualTimerServerSelectionCheckTaskID = m_manualTimerServerSelectionCheckTaskID;
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

        assert( TimeManager_instance == this );
        TimeManager_instance = nullptr;
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
                m_timeDelta = QDateTime::currentMSecsSinceEpoch() - elapsed;
                m_usedTimeSyncInfo = TimeSyncInfo(
                    elapsed,
                    elapsed + m_timeDelta,
                    m_localTimePriorityKey ); 

                if( !synchronizingByCurrentServer )
                {
                    //new sync time will be broadcasted with the next transaction (it will be at least broadcastPeerSystemTime)
                    const qint64 curSyncTime = m_monotonicClock.elapsed() + m_timeDelta;
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
        const QnId& /*remotePeerID*/,
        qint64 localMonotonicClock,
        qint64 remotePeerSyncTime,
        const TimePriorityKey& remotePeerTimePriorityKey )
    {
        assert( remotePeerTimePriorityKey.seed > 0 );

        QMutexLocker lk( &m_mutex );

        //if there is new maximum remotePeerTimePriorityKey than updating delta and emitting timeChanged
        if( remotePeerTimePriorityKey > m_usedTimeSyncInfo.timePriorityKey )
        {
            //taking new synchronization data
            m_timeDelta = remotePeerSyncTime - localMonotonicClock;
            m_usedTimeSyncInfo = TimeSyncInfo(
                localMonotonicClock,
                remotePeerSyncTime,
                remotePeerTimePriorityKey ); 
            lk.unlock();
            emit timeChanged( m_monotonicClock.elapsed() + m_timeDelta );
        }
    }

    void TimeSynchronizationManager::syncTimeWithExternalSource()
    {
        //TODO #ak synchronizing with some internet server if possible
    }

    void TimeSynchronizationManager::onNewConnectionEstablished( const QnTransactionTransportPtr& transport )
    {
        using namespace std::placeholders;
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

    void TimeSynchronizationManager::broadcastLocalSystemTime( quint64 /*taskID*/ )
    {
        {
            QMutexLocker lk( &m_mutex );
            m_broadcastSysTimeTaskID = 0;
            if( m_terminated )
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
        tran.params.peerSysTime = QDateTime::currentMSecsSinceEpoch();
        QnTransactionMessageBus::instance()->sendTransaction( tran );
    }

    void TimeSynchronizationManager::checkIfManualTimeServerSelectionIsRequired( quint64 /*taskID*/ )
    {
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
            const qint64 currentLocalTime = QDateTime::currentMSecsSinceEpoch();

            //list<pair<peerid, time> >
            QList<QPair<QnId, qint64> > peers;
            for( auto val: peersByTimePriorityFlags )
            {
                if( val.first != peersByTimePriorityFlags.cbegin()->first )
                    break;
                peers.push_back( QPair<QnId, qint64>( val.second->first, val.second->second.syncTime + (currentClock - val.second->second.monotonicClockValue) ) );
            }
            //all peers have same priority, user selection is required
            emit primaryTimeServerSelectionRequired( currentLocalTime, peers );
        }
    }

    void TimeSynchronizationManager::onRecevingHttpChunkExtensions(
        QnTransactionTransport* transport,
        const std::vector<nx_http::ChunkExtension>& extensions )
    {
        for( auto extension: extensions )
        {
            if( extension.first == TIME_SYNC_EXTENSION_NAME )
            {
                int rttMillis = 0;    //TODO #ak get tcp connection round trip time
                TimeSyncInfo remotePeerTimeSyncInfo;
                if( !remotePeerTimeSyncInfo.fromString( extension.second ) )
                    continue;
        
                remotePeerTimeSyncUpdate(
                    transport->remotePeer().id,
                    m_monotonicClock.elapsed(),
                    remotePeerTimeSyncInfo.syncTime + rttMillis / 2,
                    remotePeerTimeSyncInfo.timePriorityKey );
                return;
            }
        }
    }
}
