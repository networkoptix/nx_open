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

    TimeSynchronizationManager::TimeSynchronizationManager( Qn::PeerType peerType )
    :
        m_timeDelta( 0 )
    {
        if( peerType == Qn::PT_Server )
            m_timePriorityKey.flags |= peerIsServer;

#ifndef EDGE_SERVER
        m_timePriorityKey.flags |= peerIsNotEdgeServer;
#endif
        m_timePriorityKey.seed = (rand() | (rand() * RAND_MAX)) + 1;
        //TODO #ak handle priority key duplicates or use guid?
        if( QElapsedTimer::isMonotonic() )
            m_timePriorityKey.flags |= peerHasMonotonicClock;

        m_monotonicClock.restart();
        //initializing synchronized time with local system time
        m_timeDelta = QDateTime::currentMSecsSinceEpoch();

        m_usedTimeSyncInfo = TimeSyncInfo(
            0,
            m_timeDelta,
            m_timePriorityKey ); 

        assert( TimeManager_instance == nullptr );
        TimeManager_instance = this;

        connect( QnTransactionMessageBus::instance(), &QnTransactionMessageBus::newConnectionEstablished,
                 this, &TimeSynchronizationManager::onNewConnectionEstablished,
                 Qt::DirectConnection );
    }

    TimeSynchronizationManager::~TimeSynchronizationManager()
    {
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
            const bool synchronizingByCurrentServer = m_usedTimeSyncInfo.timePriorityKey == m_timePriorityKey;
            //TODO #ak does it mean that local system time is to be used as synchronized time?
            m_timePriorityKey.flags |= peerTimeSetByUser;
            //incrementing sequence 
            m_timePriorityKey.sequence = m_usedTimeSyncInfo.timePriorityKey.sequence + 1;
            if( m_timePriorityKey > m_usedTimeSyncInfo.timePriorityKey )
            {
                //using current server time info
                const qint64 elapsed = m_monotonicClock.elapsed();
                m_usedTimeSyncInfo = TimeSyncInfo(
                    elapsed,
                    elapsed + m_timeDelta,
                    m_timePriorityKey ); 

                if( !synchronizingByCurrentServer )
                {
                    //TODO #ak broadcasting current sync time
                    const qint64 curSyncTime = m_monotonicClock.elapsed() + m_timeDelta;
                    lk.unlock();
                    emit timeChanged( curSyncTime );
                }
            }
        }
        else
        {
            m_timePriorityKey.flags &= ~peerTimeSetByUser;
        }
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

        //const quint64 currentMaxRemotePeerTimePriorityKey = m_timeInfoByPeer.empty()
        //    ? 0     //priority key is garanteed to be non-zero
        //    : m_timeInfoByPeer.begin()->first;

        ////saving info for later use
        //m_timeInfoByPeer[remotePeerTimePriorityKey] = RemotePeerTimeInfo(
        //    peerID,
        //    localMonotonicClock,
        //    remotePeerSyncTime );

        //if there is new maximum remotePeerTimePriorityKey than updating delta and emitting timeChanged
        if( remotePeerTimePriorityKey > m_usedTimeSyncInfo.timePriorityKey )
        {
            //taking new synchronization data
            m_timeDelta = remotePeerSyncTime - localMonotonicClock;
            m_usedTimeSyncInfo = TimeSyncInfo(
                localMonotonicClock,
                remotePeerSyncTime,
                remotePeerTimePriorityKey ); 
        }
    }

    //void TimeSynchronizationManager::remotePeerLost( const QnId& peerID )
    //{
    //    QMutexLocker lk( &m_mutex );

    //    auto it = std::find_if(
    //        m_timeInfoByPeer.begin(),
    //        m_timeInfoByPeer.end(),
    //        [peerID]( const std::pair<quint64, RemotePeerTimeInfo>& val ) {
    //            return val.second.peerID == peerID;
    //        } );
    //    if( it == m_timeInfoByPeer.begin() )
    //    {
    //        //TODO #ak using another peer for synchronization
    //    }
    //    
    //    if( it != m_timeInfoByPeer.end() )
    //        m_timeInfoByPeer.erase( it );
    //}

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
        //TODO #ak synchronizing time periodically (once per several minutes) and on demand
        extensions->emplace_back( TIME_SYNC_EXTENSION_NAME, getTimeSyncInfo().toString() );
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
        
                QMutexLocker lk( &m_mutex );
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
