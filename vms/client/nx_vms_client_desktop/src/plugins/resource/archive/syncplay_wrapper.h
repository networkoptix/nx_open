// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef __SYNCPLAY_WRAPPER_H__
#define __SYNCPLAY_WRAPPER_H__

#include <utils/media/externaltimesource.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/streaming/abstract_archive_stream_reader.h>

class QnAbstractArchiveStreamReader;
class QnAbstractArchiveDelegate;
class QnArchiveSyncPlayWrapperPrivate;

class QnArchiveSyncPlayWrapper: public QObject, public QnlTimeSource, public QnAbstractNavigator
{
    Q_OBJECT
public:
    QnArchiveSyncPlayWrapper(QObject *parent = nullptr);
    virtual ~QnArchiveSyncPlayWrapper();
    void addArchiveReader(QnAbstractArchiveStreamReader* reader, QnlTimeSource* cam);

    void removeArchiveReader(QnAbstractArchiveStreamReader* reader);

    virtual qint64 getCurrentTime() const override;
    virtual qint64 getDisplayedTime() const override;
    virtual qint64 getExternalTime() const override;
    virtual qint64 getNextTime() const override;
    virtual qint64 expectedTime() const override;

    // nav delegate
    virtual void directJumpToNonKeyFrame(qint64 mksec) override;

    virtual bool jumpTo(qint64 mksec,  qint64 skipTime) override;
    virtual void setSkipFramesToTime(qint64 skipTime) override;

    virtual void previousFrame(qint64 mksec) override;
    virtual void nextFrame() override;
    virtual void pauseMedia() override;
    virtual void resumeMedia() override;
    virtual bool isMediaPaused() const override;
    //void setPlaybackMask(const QnTimePeriodList& playbackMask);

    void disableSync();
    void enableSync(qint64 currentTime, float currentSpeed);

    virtual bool isEnabled() const override;

    virtual bool isBuffering() const override;
    virtual void onBufferingFinished(QnlTimeSource* source) override;
    virtual void onBufferingStarted(QnlTimeSource* source, qint64 bufferingTime) override;

    virtual void setSpeed(double value, qint64 currentTimeHint) override;
    virtual double getSpeed() const override;

    virtual void reinitTime(qint64 newTime) override;

    void setLiveModeEnabled(bool value);

public slots:
    void jumpToLive();
    virtual void onEofReached(QnlTimeSource* src, bool value) override;
    void onConsumerBlocksReader(QnAbstractStreamDataProvider* reader, bool value);

private slots:
    void onBeforeJump(qint64 mksec);
    void onJumpOccured(qint64 mksec);

private:
    void onConsumerBlocksReaderInternal(QnAbstractArchiveStreamReader* reader, bool value);
    void reinitTimeIfBufferingIsFinishedUnsafe();
private:
    friend class QnSyncPlayArchiveDelegate;

    qint64 minTime() const;
    qint64 endTime() const;

    void erase(QnAbstractArchiveDelegate* value);
    qint64 getDisplayedTimeInternal(bool useEofItems = false) const;
    qint64 findTimeAtPlaybackMask(qint64 timeUsec);
    void setJumpTime(qint64 mksec);
    qint64 maxArchiveTime() const;
    qint64 getCurrentTimeInternal() const;

protected:
    Q_DECLARE_PRIVATE(QnArchiveSyncPlayWrapper);

    QnArchiveSyncPlayWrapperPrivate *d_ptr;
};

#endif // __SYNCPLAY_WRAPPER_H__
