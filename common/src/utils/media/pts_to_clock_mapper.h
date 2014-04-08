/**********************************************************
* 28 mar 2014
* akolesnikov@networkoptix.com
***********************************************************/

#ifndef PTS_TO_CLOCK_MAPPER_H
#define PTS_TO_CLOCK_MAPPER_H

#include <QtCore/QMutex>


/*!
    - pts
    - absolute time on source. This time is used to synchronize multiple streams
    - local time (\a qnSyncTime->currentMSecsSinceEpoch())

    \note This class is NOT thread-safe
*/
class PtsToClockMapper
{
public:
    typedef int32_t pts_type;

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
        mutable QMutex m_timestampSynchronizationMutex;
        //!Difference (usec) between local time and source time
        int64_t m_localToSourceTimeShift;
        size_t m_modificationSequence;
    };

    PtsToClockMapper(
        pts_type ptsFrequency,
        TimeSynchronizationData* timeSynchro,
        int sourceID = -1 );

    /*!
        \return Synchronized absolute local time (usec) corresponding to frame generation
    */
    int64_t getTimestamp( pts_type pts );
    /*!
        \a localTimeOnSource Usec. Absolute time on source corresponding to \a pts
    */
    void updateTimeMapping( pts_type pts, int64_t localTimeOnSource );

private:
    //void resyncTime( int32_t pts, int64_t absoluteSourceTimeUsec );

    const int64_t m_ptsFrequency;
    TimeSynchronizationData* const m_timeSynchro;
    const int m_sourceID;
    pts_type m_prevPts;
    pts_type m_ptsBase;
    int64_t m_baseClock;
    int64_t m_baseClockOnSource;
    size_t m_sharedSynchroModificationSequence;
    bool m_prevPtsValid;
};

#endif  //PTS_TO_CLOCK_MAPPER_H
