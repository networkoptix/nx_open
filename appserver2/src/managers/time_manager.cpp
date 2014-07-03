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


namespace ec2
{
    TimeManager::TimeManager()
    :
        m_timeDelta( 0 ),
        m_timePriorityKey( 0 )
    {
#ifndef EDGE_SERVER
        m_timePriorityKey |= peerIsNotEdgeServer;
#endif
        m_timePriorityKey |= (rand() | (rand() * RAND_MAX)) + 1;
        //TODO #ak handle priority key duplicates or use guid?
        if( QElapsedTimer::isMonotonic() )
            m_timePriorityKey |= peerHasMonotonicClock;

        m_monotonicClock.restart();
        //initializing synchronized time with local system time
        m_timeDelta = QDateTime::currentMSecsSinceEpoch();

        m_usedTimeSyncInfo = TimeSyncInfo(
            0,
            m_timeDelta,
            m_timePriorityKey ); 
    }

    TimeManager::~TimeManager()
    {
    }

    int TimeManager::getSyncTime( qint64* const syncTime ) const
    {
        QMutexLocker lk( &m_mutex );
        
        const int reqID = generateRequestID();

        *syncTime = m_monotonicClock.elapsed() + m_timeDelta;

        //TODO
        //return ec2::ErrorCode::notImplemented;
        return reqID;
    }

    int TimeManager::forcePrimaryTimeServer( const QnId& serverGuid, impl::SimpleHandlerPtr handler )
    {
        QMutexLocker lk( &m_mutex );

        const int reqID = generateRequestID();

        //TODO sending transaction (new one)
        QnTransaction<ApiPrimaryTimeServerData> tran;
        tran.params.serverGuid = serverGuid;
        tran.persistent = false;

        QtConcurrent::run( std::bind( &impl::SimpleHandler::done, handler, reqID, ec2::ErrorCode::notImplemented ) );
        return reqID;
    }

    void TimeManager::primaryTimeServerChanged( const QnTransaction<ApiPrimaryTimeServerData>& tran )
    {
        QMutexLocker lk( &m_mutex );

        //received transaction

        if( tran.params.serverGuid == qnCommon->moduleGUID() )
        {
            const bool synchronizingByCurrentServer = m_usedTimeSyncInfo.timePriorityKey == m_timePriorityKey;
            //local peer is selected by user as primary time server
            //TODO #ak does it mean that local system time is to be used as synchronized time?
            m_timePriorityKey |= peerTimeSetByUser;
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
                    emit timeChanged( m_monotonicClock.elapsed() + m_timeDelta );
                }
            }
        }
        else
        {
            m_timePriorityKey &= ~peerTimeSetByUser;
        }
    }

    TimeSyncInfo TimeManager::getTimeSyncInfo() const
    {
        QMutexLocker lk( &m_mutex );

        const qint64 elapsed = m_monotonicClock.elapsed();
        return TimeSyncInfo(
            elapsed,
            elapsed + m_timeDelta,
            m_usedTimeSyncInfo.timePriorityKey );
    }

    void TimeManager::remotePeerTimeSyncUpdate(
        const QnId& /*peerID*/,
        qint64 localMonotonicClock,
        qint64 remotePeerSyncTime,
        quint64 remotePeerTimePriorityKey )
    {
        assert( remotePeerTimePriorityKey > 0 );

        QMutexLocker lk( &m_mutex );

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

    //void TimeManager::remotePeerLost( const QnId& peerID )
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

    void TimeManager::syncTimeWithExternalSource()
    {
        //TODO #ak synchroniing with some internet server if possible
    }
}
