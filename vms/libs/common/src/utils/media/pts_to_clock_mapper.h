#pragma once

#include <nx/utils/thread/mutex.h>


/*!
    - pts
    - absolute time on source. This time is used to synchronize multiple streams
    - local time (\a qnSyncTime->currentMSecsSinceEpoch())

    \note This class is NOT thread-safe
*/
class PtsToClockMapper
{
public:
    typedef uint32_t pts_type;
    typedef int64_t ts_type;

    //!Used for synchronizing multiple streams with same source
    /*!
        This class is thread-safe and can be used by different threads
    */
    class TimeSynchronizationData
    {
        friend class PtsToClockMapper;

    public:
        TimeSynchronizationData();

        void mapLocalTimeToSourceAbsoluteTime( int64_t localTimeUSec, int64_t sourceTimeUSec );
        //!(usec)
        int64_t localToSourceTimeShift() const;
        size_t modificationSequence() const;

    private:
        mutable QnMutex m_timestampSynchronizationMutex;
        //!Difference (usec) between local time and source time
        int64_t m_localToSourceTimeShift;
        size_t m_modificationSequence;
    };

    /*!
        \param ptsFrequency Number of pts ticks per second
    */
    PtsToClockMapper(
        pts_type ptsFrequency,
        size_t ptsBits,
        TimeSynchronizationData* timeSynchro,
        int sourceID = -1 );

    /*!
        \return Synchronized absolute local time (usec) corresponding to frame generation
    */
    ts_type getTimestamp( pts_type pts );
    //!Resets timestamp calculation to supplied values
    /*!
        \a localTimeOnSource Usec. Absolute time on source corresponding to \a pts
        \note \a pts MUST be close to current pts generator value. I.e., difference between \a pts and 
            next pts reported to \a PtsToClockMapper::getTimestamp MUST not overflow
    */
    void updateTimeMapping( pts_type pts, int64_t localTimeOnSourceUSec );
    pts_type ptsDeltaInCaseOfDiscontinuity() const;

private:
    const int64_t m_ptsFrequency;
    const size_t m_ptsBits;
    const pts_type m_ptsMask;
    const pts_type m_maxPtsDrift;
    const pts_type m_ptsDeltaInCaseOfDiscontinuity;
    int m_ptsOverflowCount;
    TimeSynchronizationData* const m_timeSynchro;
    const int m_sourceID;
    pts_type m_prevPts;
    ts_type m_baseClock;
    ts_type m_baseClockOnSource;
    size_t m_sharedSynchroModificationSequence;
    bool m_prevPtsValid;
    pts_type m_correction;

    pts_type absDiff(
        PtsToClockMapper::pts_type one,
        PtsToClockMapper::pts_type two ) const;
    void recalcPtsCorrection( pts_type ptsCorrection, pts_type* const pts );
};
