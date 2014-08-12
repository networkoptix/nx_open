#ifndef __PLAYBACK_MASK_HELPER_H__
#define __PLAYBACK_MASK_HELPER_H__

#ifdef ENABLE_DATA_PROVIDERS

#include <recording/time_period.h>
#include <recording/time_period_list.h>

class QnPlaybackMaskHelper
{
public:
    QnPlaybackMaskHelper();

    qint64 findTimeAtPlaybackMask(qint64 timeUsec, bool isForwardDirection);
    void setPlaybackMask(const QnTimePeriodList& playbackMask);
    void setPlaybackRange(const QnTimePeriod& playbackRange);
    QnTimePeriod getPlaybackRange() const;
    
    qint64 endTimeMs() const;

private:
    QnTimePeriodList m_playbackMask;
    QnTimePeriodList m_playBackMaskOrig;
    QnTimePeriod m_playbackRange;
    QnTimePeriod m_curPlaybackPeriod;
};

#endif //ENABLE_DATA_PROVIDERS

#endif // __PLAYBACK_MASK_HELPER_H__
