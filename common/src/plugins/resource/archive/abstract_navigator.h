#ifndef QN_ABSTRACT_NAVIGATOR_H
#define QN_ABSTRACT_NAVIGATOR_H

#ifdef ENABLE_ARCHIVE

#include <QtCore/QtGlobal>

class QnAbstractNavigator
{
public:
    virtual ~QnAbstractNavigator() {}

    virtual void directJumpToNonKeyFrame(qint64 mksec) = 0;
    virtual bool jumpTo(qint64 mksec,  qint64 skipTime) = 0;
    virtual void setSkipFramesToTime(qint64 skipTime) = 0;
    virtual void previousFrame(qint64 mksec) = 0;
    virtual void nextFrame() = 0;
    virtual void pauseMedia() = 0;
    virtual void resumeMedia() = 0;

    virtual bool isMediaPaused() const = 0;

    //virtual void setPlaybackMask(const QnTimePeriodList& playbackMask) = 0;

    virtual bool isEnabled() const { return true; }

    virtual void setSpeed(double value, qint64 currentTimeHint) = 0;

    // playback filter by motion detection mask
    // delivery motion data to a client
    //virtual bool setSendMotion(bool value) = 0;
};

#endif // ENABLE_ARCHIVE

#endif // QN_ABSTRACT_NAVIGATOR_H
