#include "pts_to_clock_mapper.h"

#include <limits>

#include <nx/utils/log/log.h>
#include <nx/utils/random.h>
#include <nx/utils/thread/mutex.h>

//#define DEBUG_OUTPUT


static const int32_t MAX_PTS_DRIFT_MS = 1000;    //1 second
static const int32_t DEFAULT_FRAME_DURATION_MS = 33;    //30 fps
static const int64_t MSEC_IN_SEC = 1000;
static const int64_t USEC_IN_SEC = 1000 * MSEC_IN_SEC;
//static const int64_t USEC_IN_MSEC = 1000;


PtsToClockMapper::TimeSynchronizationData::TimeSynchronizationData()
:
    m_localToSourceTimeShift( 0 ),
    m_modificationSequence( 1 )
{
}

void PtsToClockMapper::TimeSynchronizationData::mapLocalTimeToSourceAbsoluteTime( int64_t localTimeUSec, int64_t sourceTimeUSec )
{
    QnMutexLocker lk( &m_timestampSynchronizationMutex );
    m_localToSourceTimeShift = localTimeUSec - sourceTimeUSec;
    ++m_modificationSequence;
#ifdef DEBUG_OUTPUT
    std::cout<<"TimeSynchronizationData::mapLocalTimeToSourceAbsoluteTime. "
        "localTimeUSec "<<localTimeUSec<<" sourceTimeUSec "<<sourceTimeUSec<<std::endl;
#endif
}

int64_t PtsToClockMapper::TimeSynchronizationData::localToSourceTimeShift() const
{
    QnMutexLocker lk( &m_timestampSynchronizationMutex );
    return m_localToSourceTimeShift;
}

size_t PtsToClockMapper::TimeSynchronizationData::modificationSequence() const
{
    QnMutexLocker lk( &m_timestampSynchronizationMutex );
    return m_modificationSequence;
}


PtsToClockMapper::PtsToClockMapper(
    pts_type ptsFrequency,
    size_t ptsBits,
    PtsToClockMapper::TimeSynchronizationData* timeSynchro,
    int sourceID )
:
    m_ptsFrequency( ptsFrequency ),
    m_ptsBits(ptsBits),
    m_ptsMask(
        (ptsBits >= sizeof(pts_type)*CHAR_BIT)
            ? std::numeric_limits<pts_type>::max()
            : ((1 << ptsBits) - 1) ),
    m_maxPtsDrift( MAX_PTS_DRIFT_MS * ptsFrequency / MSEC_IN_SEC ),
    m_ptsDeltaInCaseOfDiscontinuity( ptsFrequency * DEFAULT_FRAME_DURATION_MS / MSEC_IN_SEC ),
    m_ptsOverflowCount( 0 ),
    m_timeSynchro( timeSynchro ),
    m_sourceID( sourceID == -1 ? nx::utils::random::number() : sourceID ),
    m_prevPts( 0 ),
    m_baseClock( 0 ),
    m_baseClockOnSource( 0 ),
    m_sharedSynchroModificationSequence( 0 ),
    m_prevPtsValid( false ),
    m_correction( 0 )
{
    NX_ASSERT( ptsBits <= sizeof(pts_type)*CHAR_BIT );
}

PtsToClockMapper::ts_type PtsToClockMapper::getTimestamp( pts_type pts )
{
    NX_ASSERT( pts <= m_ptsMask );

    pts = (pts - m_correction) & m_ptsMask;

    if( !m_prevPtsValid )
    {
        m_prevPts = pts;
        m_prevPtsValid = true;
    }

    while( m_sharedSynchroModificationSequence != m_timeSynchro->modificationSequence() )
    {
        m_baseClock = m_baseClockOnSource + m_timeSynchro->localToSourceTimeShift();
        m_sharedSynchroModificationSequence = m_timeSynchro->modificationSequence();
#ifdef DEBUG_OUTPUT
        std::cout<<"stream "<<m_sourceID<<". Updated time mapping. "<<std::endl;
#endif
    }

    pts_type ptsDelta = (pts - m_prevPts) & m_ptsMask;

    //calculating abs diff of pts and m_prevPts
    if( absDiff( pts, m_prevPts ) > m_maxPtsDrift )
    {
#ifdef DEBUG_OUTPUT
        std::cout<<"stream "<<m_sourceID<<". discontinuity found"<<std::endl;
#endif
        //pts discontinuity. Considering as if pts grows by m_ptsDeltaInCaseOfDiscontinuity
        NX_WARNING(this, "Stream %1. Pts discontinuity (current %2, prev %3)",
            m_sourceID, pts, m_prevPts);
        const pts_type localCorrection = (ptsDelta - m_ptsDeltaInCaseOfDiscontinuity) & m_ptsMask;
        recalcPtsCorrection( localCorrection, &pts );
        m_prevPts = (pts - m_ptsDeltaInCaseOfDiscontinuity) & m_ptsMask;
        ptsDelta = (pts - m_prevPts) & m_ptsMask;
    }

    //checking for overflow
    if( (ptsDelta < m_maxPtsDrift) && (pts < m_prevPts) )
    {
        //pts overflow
        ++m_ptsOverflowCount;
    }

    if( (ptsDelta > (m_ptsMask >> 1))   //ptsDelta is negative
        && (pts > m_prevPts) )          //overflow between pts and m_prevPts
    {
        //pts underflow
        --m_ptsOverflowCount;
    }

    m_prevPts = pts;
    const ts_type totalPtsChange =
           (ts_type)(pts & m_ptsMask)                       //current pts
           + m_ptsOverflowCount * ((ts_type)m_ptsMask+1);   //overflows

    const ts_type currentTimestamp =
        m_baseClock + totalPtsChange * USEC_IN_SEC / m_ptsFrequency;

    NX_VERBOSE(this, "pts %1, timestamp %2, %3", pts, currentTimestamp, m_sourceID);
#ifdef DEBUG_OUTPUT
    std::cout<<"stream "<<m_sourceID<<". pts "<<pts<<", ts "<<currentTimestamp<<std::endl;
#endif
    return currentTimestamp;
}

void PtsToClockMapper::updateTimeMapping( const pts_type pts, int64_t localTimeOnSourceUsec )
{
    NX_ASSERT(pts <= m_ptsMask);

    m_baseClockOnSource = localTimeOnSourceUsec;
    m_baseClock = m_baseClockOnSource + m_timeSynchro->localToSourceTimeShift();

    m_ptsOverflowCount = 0;
    //we need correction so that pts always overflow around zero (pts base can be nonzero)
    m_correction = pts;
    m_prevPts = 0;  //(pts - m_correction) & m_ptsMask;
    m_prevPtsValid = true;

#ifdef DEBUG_OUTPUT
    std::cout<<"stream "<<m_sourceID<<". PtsToClockMapper::updateTimeMapping. "
        "ptsBase "<<pts<<", localTimeOnSourceUsec "<<localTimeOnSourceUsec<<", m_baseClock "<<m_baseClock<<
        ", m_timeSynchro->localToSourceTimeShift() "<<m_timeSynchro->localToSourceTimeShift()<<std::endl;
#endif
}

PtsToClockMapper::pts_type PtsToClockMapper::ptsDeltaInCaseOfDiscontinuity() const
{
    return m_ptsDeltaInCaseOfDiscontinuity;
}

PtsToClockMapper::pts_type PtsToClockMapper::absDiff(
    PtsToClockMapper::pts_type one,
    PtsToClockMapper::pts_type two ) const
{
    return std::min<pts_type>((one - two) & m_ptsMask, (two - one) & m_ptsMask);
}

void PtsToClockMapper::recalcPtsCorrection(
    PtsToClockMapper::pts_type ptsCorrection,
    pts_type* const pts )
{
    m_prevPts = (m_prevPts - ptsCorrection) & m_ptsMask;
    *pts = (*pts - ptsCorrection) & m_ptsMask;

    m_correction = (m_correction + ptsCorrection) & m_ptsMask;
}
