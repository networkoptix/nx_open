/**********************************************************
* 28 mar 2014
* akolesnikov@networkoptix.com
***********************************************************/

#include "pts_to_clock_mapper.h"

#include <QtCore/QMutexLocker>

#include <utils/common/log.h>


static const int MAX_FRAMES_BETWEEN_TIME_RESYNC = 300;
static const unsigned int MAX_PTS_DRIFT = 63000;    //700 ms
static const unsigned int DEFAULT_FRAME_DURATION = 3000;    //30 fps
static const int MSEC_IN_SEC = 1000;
static const int USEC_IN_SEC = 1000*1000;
static const int USEC_IN_MSEC = 1000;


PtsToClockMapper::TimeSynchronizationData::TimeSynchronizationData()
:
    m_localToSourceTimeShift( 0 ),
    m_modificationSequence( 1 )
{
}

void PtsToClockMapper::TimeSynchronizationData::mapLocalTimeToSourceAbsoluteTime( int64_t localTimeUSec, int64_t sourceTimeUSec )
{
    QMutexLocker lk( &m_timestampSynchronizationMutex );
    m_localToSourceTimeShift = localTimeUSec - sourceTimeUSec;
    ++m_modificationSequence;
}

int64_t PtsToClockMapper::TimeSynchronizationData::localToSourceTimeShift() const
{
    QMutexLocker lk( &m_timestampSynchronizationMutex );
    return m_localToSourceTimeShift;
}

size_t PtsToClockMapper::TimeSynchronizationData::modificationSequence() const
{
    QMutexLocker lk( &m_timestampSynchronizationMutex );
    return m_modificationSequence;
}


PtsToClockMapper::PtsToClockMapper(
    pts_type ptsFrequency,
    PtsToClockMapper::TimeSynchronizationData* timeSynchro,
    int sourceID )
:
    m_ptsFrequency( ptsFrequency ),
    m_timeSynchro( timeSynchro ),
    m_sourceID( sourceID ? sourceID : rand() ),
    m_prevPts( 0 ),
    m_ptsBase( 0 ),
    m_baseClock( 0 ),
    m_baseClockOnSource( 0 ),
    m_sharedSynchroModificationSequence( 0 )
{
}

int64_t PtsToClockMapper::getTimestamp( pts_type pts )
{
    while( m_sharedSynchroModificationSequence != m_timeSynchro->modificationSequence() )
    {
        updateTimeMapping( m_ptsBase, m_baseClockOnSource );
        m_sharedSynchroModificationSequence = m_timeSynchro->modificationSequence();
    }

    if (qAbs((int32_t) (pts - m_prevPts)) > MAX_PTS_DRIFT)
    {
        //pts discontinuity
        NX_LOG( lit("Stream %1. Pts discontinuity (current %2, prev %3)").arg(m_sourceID).arg(pts).arg(m_prevPts), cl_logWARNING );
        m_ptsBase += pts - m_prevPts - DEFAULT_FRAME_DURATION;
    }

    m_prevPts = pts;
    const int64_t currentTimestamp = m_baseClock + (pts - m_ptsBase) * USEC_IN_SEC / m_ptsFrequency;

    NX_LOG( lit("pts %1, timestamp %2, %3").arg(pts).arg(currentTimestamp).arg(m_sourceID), cl_logDEBUG2 );
    return currentTimestamp;
}

void PtsToClockMapper::updateTimeMapping( pts_type pts, int64_t localTimeOnSourceUsec )
{
    m_ptsBase = pts;
    m_baseClockOnSource = localTimeOnSourceUsec;
    m_baseClock = m_baseClockOnSource + m_timeSynchro->localToSourceTimeShift();
}
