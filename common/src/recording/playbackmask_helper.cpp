#include "playbackmask_helper.h"

extern "C"
{
    #include <libavutil/avutil.h> /* For AV_NOPTS_VALUE. */
}

#include <utils/common/util.h>


QnPlaybackMaskHelper::QnPlaybackMaskHelper()
{

}

qint64 QnPlaybackMaskHelper::findTimeAtPlaybackMask(qint64 timeUsec, bool isForwardDirection)
{
    qint64 timeMs = timeUsec/1000;
    if (m_playbackMask.isEmpty() || m_curPlaybackPeriod.contains(timeMs))
        return timeUsec;
    QnTimePeriodList::const_iterator itr = m_playbackMask.findNearestPeriod(timeMs, isForwardDirection);
    if (itr == m_playbackMask.constEnd()) 
        return isForwardDirection ? DATETIME_NOW : 0;
    
    m_curPlaybackPeriod = *itr;
    if (!m_curPlaybackPeriod.contains(timeMs)) {
        if (isForwardDirection)
            return m_curPlaybackPeriod.startTimeMs*1000;
        else {
            if (m_curPlaybackPeriod.startTimeMs*1000 > timeUsec)
                return -1; // BOF reached
            else
                return (m_curPlaybackPeriod.startTimeMs + m_curPlaybackPeriod.durationMs)*1000 - BACKWARD_SEEK_STEP;
        }
    }
    else {
        return timeUsec;
    }
}

QnTimePeriod QnPlaybackMaskHelper::getPlaybackRange() const
{
    return m_playbackRange;
}

void QnPlaybackMaskHelper::setPlaybackRange(const QnTimePeriod& playbackRange)
{
    m_playbackRange = playbackRange;
    
    if (m_playbackRange.isEmpty())
        m_playbackMask = m_playBackMaskOrig;
    else if (m_playbackMask.isEmpty())
        m_playbackMask << playbackRange;
    else
        m_playbackMask = m_playbackMask.intersected(playbackRange);
    m_curPlaybackPeriod.clear();

}

void QnPlaybackMaskHelper::setPlaybackMask(const QnTimePeriodList& playbackMask)
{
    m_playBackMaskOrig = playbackMask;

    if (m_playbackRange.isEmpty())
        m_playbackMask = playbackMask;
    else {
        m_playbackMask = playbackMask.intersected(m_playbackRange);
        if (m_playbackMask.isEmpty() && !m_playbackRange.isEmpty())
            m_playbackMask << m_playbackRange;
    }
    m_curPlaybackPeriod.clear();
}

qint64 QnPlaybackMaskHelper::endTimeMs() const
{
    if (m_playbackMask.isEmpty())
        return AV_NOPTS_VALUE;
    else
        return m_playbackMask.last().endTimeMs();
}
