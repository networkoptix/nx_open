#include "playbackmask_helper.h"
#include "utils/common/util.h"

QnPlaybackMaskHelper::QnPlaybackMaskHelper()
{

}

qint64 QnPlaybackMaskHelper::findTimeAtPlaybackMask(qint64 timeUsec, bool isForwardDirection)
{
    qint64 timeMs = timeUsec/1000;
    if (m_playbackMask.isEmpty() || m_curPlaybackPeriod.containTime(timeMs))
        return timeUsec;
    QnTimePeriodList::const_iterator itr = m_playbackMask.findNearestPeriod(timeMs, isForwardDirection);
    if (itr == m_playbackMask.end()) {
        m_playbackMask.findNearestPeriod(timeMs, isForwardDirection);
        return DATETIME_NOW;
    }
    m_curPlaybackPeriod = *itr;
    if (!m_curPlaybackPeriod.containTime(timeMs))
        return isForwardDirection ? m_curPlaybackPeriod.startTimeMs*1000 : (m_curPlaybackPeriod.startTimeMs + m_curPlaybackPeriod.durationMs)*1000 - BACKWARD_SEEK_STEP;
    else
        return timeUsec;
}

void QnPlaybackMaskHelper::setPlaybackMask(const QnTimePeriodList& playbackMask)
{
    m_playbackMask = playbackMask;
    m_curPlaybackPeriod.clear();
}
