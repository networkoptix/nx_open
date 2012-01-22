#ifndef __SYNCPLAY_WRAPPER_H__
#define __SYNCPLAY_WRAPPER_H__

#include "utils/common/base.h"
#include "utils/media/externaltimesource.h"
#include "core/dataprovider/abstract_streamdataprovider.h"
#include "abstract_archive_stream_reader.h"
#include "recording/time_period.h"

class QnAbstractArchiveReader;
class QnAbstractArchiveDelegate;
class QnlTimeSource;

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
    virtual bool jumpTo(qint64 mksec,  qint64 skipTime);
    virtual void previousFrame(qint64 mksec);
    virtual void nextFrame();
    virtual void pauseMedia();
    virtual void resumeMedia();
    virtual void setSingleShotMode(bool single);
    virtual bool isMediaPaused() const;
    //void setPlaybackMask(const QnTimePeriodList& playbackMask);


    void setEnabled(bool value);
    virtual bool isEnabled() const;
    //virtual bool setMotionRegion(const QRegion& region);
    //virtual bool setSendMotion(bool value);
    //

public slots:
    void onBufferingStarted(QnlTimeSource* source);
    void onBufferingFinished(QnlTimeSource* source);
    void onConsumerBlocksReader(QnAbstractStreamDataProvider* reader, bool value);
private slots:
    void onBeforeJump(qint64 mksec);
    void onJumpOccured(qint64 mksec);
    void onJumpCanceled(qint64 time);
    void onSpeedChanged(double value);
private:
    qint64 minTime() const;
    qint64 endTime() const;
    qint64 secondTime() const;
    //void waitIfNeed(QnAbstractArchiveReader* reader, qint64 timestamp);
    void erase(QnAbstractArchiveDelegate* value);
    void reinitTime(qint64 newTime);
    qint64 getDisplayedTimeInternal() const;
    qint64 findTimeAtPlaybackMask(qint64 timeUsec);
private:
    friend class QnSyncPlayArchiveDelegate;
    QN_DECLARE_PRIVATE(QnArchiveSyncPlayWrapper);
};

#endif // __SYNCPLAY_WRAPPER_H__
