#ifndef abstract_archive_stream_reader_h1907
#define abstract_archive_stream_reader_h1907

#include "abstract_archive_delegate.h"
#include "utils/common/adaptive_sleep.h"
#include "core/dataprovider/cpull_media_stream_provider.h"
#include "recording/time_period.h"

class QnAbstractNavigator
{
public:
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

class QnAbstractArchiveReader : public QnAbstractMediaStreamDataProvider, public QnAbstractNavigator
{
    Q_OBJECT
public:
    QnAbstractArchiveReader(QnResourcePtr dev);
    virtual ~QnAbstractArchiveReader();

    void setNavDelegate(QnAbstractNavigator* navDelegate);

    QnAbstractArchiveDelegate* getArchiveDelegate() const;

    virtual bool isSingleShotMode() const = 0;


    // Manual open. Open will be called automatically on first data access
    bool open();

    /**
     * \returns                         Current position of this reader, in
     *                                  microseconds.
     */
    virtual qint64 currentTime() const = 0;
    virtual bool isSkippingFrames() const = 0;

    /**
     * \returns                         Length of archive, in microseconds.
     */
    quint64 lengthUsec() const;

    virtual void directJumpToNonKeyFrame(qint64 usec) = 0;
    virtual void setSkipFramesToTime(qint64 skipTime) = 0;
    void jumpToPreviousFrame(qint64 usec);


    // gives a list of audio tracks
    virtual QStringList getAudioTracksInfo() const;

    //gets current audio channel ( if we have the only channel - returns 0 )
    virtual unsigned int getCurrentAudioChannel() const;

    // sets certain track
    virtual bool setAudioChannel(unsigned int num);
    virtual bool isNegativeSpeedSupported() const = 0;

    void setCycleMode(bool value);

    /**
     * \returns                         Time of the archive's start, in microseconds.
     */
    virtual qint64 startTime() const = 0;

    /**
     * \returns                         Time of the archive's end, in microseconds.
     */
    virtual qint64 endTime() const = 0;

    bool isRealTimeSource() const;

    virtual bool setSendMotion(bool value) = 0;
    virtual void setPlaybackMask(const QnTimePeriodList& playbackMask) = 0;
    virtual void setPlaybackRange(const QnTimePeriod& playbackRange) = 0;
    virtual QnTimePeriod getPlaybackRange() const = 0;

    virtual void setQuality(MediaQuality quality, bool fastSwitch) = 0;
    virtual MediaQuality getQuality() const = 0;

    virtual void setArchiveDelegate(QnAbstractArchiveDelegate* contextDelegate) = 0;

    virtual void run() override;

    virtual void setSpeed(double value, qint64 currentTimeHint = AV_NOPTS_VALUE) = 0;
    virtual double getSpeed() const = 0;

    virtual void startPaused() = 0;
    virtual void setRole(QnResource::ConnectionRole role) override {}
signals:
    void beforeJump(qint64 mksec);
    void jumpOccured(qint64 mksec);
    void jumpCanceled(qint64 mksec);
    void streamPaused();
    void streamResumed();
    void nextFrameOccured();
    void prevFrameOccured();
    void skipFramesTo(qint64 mksec);
protected:
    bool m_cycleMode;
    qint64 m_needToSleep;
    QnAbstractArchiveDelegate* m_delegate;
    QnAbstractNavigator* m_navDelegate;
};

#endif //abstract_archive_stream_reader_h1907
