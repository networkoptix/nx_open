#pragma once

#include <QtCore/QtGlobal>

extern "C"
{
#include <libavutil/avutil.h>
}

class QnAbstractNavigator
{
public:
    virtual ~QnAbstractNavigator() = default;

    virtual void directJumpToNonKeyFrame(qint64 mksec) = 0;
    virtual bool jumpTo(qint64 mksec, qint64 skipTime) = 0;
    virtual void setSkipFramesToTime(qint64 skipTime) = 0;
    virtual void previousFrame(qint64 mksec) = 0;
    virtual void nextFrame() = 0;
    virtual void pauseMedia() = 0;
    virtual void resumeMedia() = 0;

    virtual bool isMediaPaused() const = 0;

    //virtual void setPlaybackMask(const QnTimePeriodList& playbackMask) = 0;

    virtual bool isEnabled() const { return true; }

    virtual void setSpeed(double value, qint64 currentTimeHint = AV_NOPTS_VALUE) = 0;
};
