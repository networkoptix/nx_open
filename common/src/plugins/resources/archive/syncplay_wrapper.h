#ifndef __SYNCPLAY_WRAPPER_H__
#define __SYNCPLAY_WRAPPER_H__

#include "utils/common/base.h"
#include "utils/media/externaltimesource.h"
#include "core/dataprovider/abstract_streamdataprovider.h"


class QnAbstractArchiveReader;
class QnAbstractArchiveDelegate;
class QnlTimeSource;

class QnArchiveSyncPlayWrapper: public QObject, public QnlTimeSource
{
    Q_OBJECT
public:
    QnArchiveSyncPlayWrapper();
    virtual ~QnArchiveSyncPlayWrapper();
    void addArchiveReader(QnAbstractArchiveReader* reader, QnlTimeSource* cam);
    void removeArchiveReader(QnAbstractArchiveReader* reader);

    virtual qint64 getCurrentTime() const;
    virtual qint64 getDisplayedTime() const;
    virtual qint64 getNextTime() const;

    //virtual void onAvailableTime(QnlTimeSource* source, qint64 time);
    void onBufferingStarted(QnlTimeSource* source);
    void onBufferingFinished(QnlTimeSource* source);
public slots:
    void onConsumerBlocksReader(QnAbstractStreamDataProvider* reader, bool value);
private slots:
    void onSingleShotModeChanged(bool value);
    void onBeforeJump(qint64 mksec, bool makeshot);
    void onJumpOccured(qint64 mksec);
    void onStreamPaused();
    void onStreamResumed();
    void onNextFrameOccured();
    void onPrevFrameOccured();
    void onSpeedChanged(double value);
private:
    qint64 minTime() const;
    qint64 endTime() const;
    //qint64 seek (qint64 time);
    qint64 secondTime() const;
    //void waitIfNeed(QnAbstractArchiveReader* reader, qint64 timestamp);
    void onNewDataReaded();
    void erase(QnAbstractArchiveDelegate* value);
    void reinitTime(qint64 newTime);
    qint64 getDisplayedTimeInternal() const;
private:
    friend class QnSyncPlayArchiveDelegate;
    QN_DECLARE_PRIVATE(QnArchiveSyncPlayWrapper);
};

#endif // __SYNCPLAY_WRAPPER_H__
