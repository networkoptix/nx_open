#ifndef __SYNCPLAY_WRAPPER_H__
#define __SYNCPLAY_WRAPPER_H__

#include <utils/media/externaltimesource.h>
#include <core/dataprovider/abstract_streamdataprovider.h>
#include <plugins/resource/archive/abstract_archive_stream_reader.h>

class QnAbstractArchiveReader;
class QnAbstractArchiveDelegate;
class QnArchiveSyncPlayWrapperPrivate;

class QnArchiveSyncPlayWrapper: public QObject, public QnlTimeSource, public QnAbstractNavigator
{
    Q_OBJECT
public:
    QnArchiveSyncPlayWrapper();
    virtual ~QnArchiveSyncPlayWrapper();
    void addArchiveReader(QnAbstractArchiveReader* reader, QnlTimeSource* cam);
    
    void removeArchiveReader(QnAbstractArchiveReader* reader);

    virtual qint64 getCurrentTime() const;
    virtual qint64 getDisplayedTime() const;
    virtual qint64 getExternalTime() const;
    virtual qint64 getNextTime() const;
    virtual qint64 expectedTime() const;

    // nav delegate
    virtual void directJumpToNonKeyFrame(qint64 mksec);

    virtual bool jumpTo(qint64 mksec,  qint64 skipTime) override;
    virtual void setSkipFramesToTime(qint64 skipTime) override;

    virtual void previousFrame(qint64 mksec);
    virtual void nextFrame();
    virtual void pauseMedia();
    virtual void resumeMedia();
    virtual bool isMediaPaused() const;
    //void setPlaybackMask(const QnTimePeriodList& playbackMask);

    void disableSync();
    void enableSync(qint64 currentTime, float currentSpeed);

    virtual bool isEnabled() const;
    virtual qreal getSpeed() const;

    //virtual bool setMotionRegion(const QRegion& region);
    //virtual bool setSendMotion(bool value);
    //
    bool isBuffering() const;

    virtual void onBufferingFinished(QnlTimeSource* source) override;
    virtual void onBufferingStarted(QnlTimeSource* source, qint64 bufferingTime) override;
    virtual void setSpeed(double value, qint64 currentTimeHint) override;

    virtual void reinitTime(qint64 newTime) override;

    void setLiveModeEnabled(bool value);

public slots:
    void jumpToLive();
    void onEofReached(QnlTimeSource* src, bool value);
    void onConsumerBlocksReader(QnAbstractStreamDataProvider* reader, bool value);

private slots:
    void onBeforeJump(qint64 mksec);
    void onJumpOccured(qint64 mksec);
    void onJumpCanceled(qint64 time);

private:
    void onConsumerBlocksReaderInternal(QnAbstractArchiveReader* reader, bool value);
private:
    friend class QnSyncPlayArchiveDelegate;

    qint64 minTime() const;
    qint64 endTime() const;
    qint64 secondTime() const;
    //void waitIfNeed(QnAbstractArchiveReader* reader, qint64 timestamp);
    void erase(QnAbstractArchiveDelegate* value);
    qint64 getDisplayedTimeInternal() const;
    qint64 findTimeAtPlaybackMask(qint64 timeUsec);
    void setJumpTime(qint64 mksec);
    qint64 maxArchiveTime() const;
    qint64 getCurrentTimeInternal() const;

protected:
    Q_DECLARE_PRIVATE(QnArchiveSyncPlayWrapper);

    QnArchiveSyncPlayWrapperPrivate *d_ptr;
};

#endif // __SYNCPLAY_WRAPPER_H__
