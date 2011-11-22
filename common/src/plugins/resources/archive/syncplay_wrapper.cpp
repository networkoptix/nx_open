#include <QWaitCondition>

#include "syncplay_wrapper.h"
#include "syncplay_archive_delegate.h"
#include "abstract_archive_stream_reader.h"
#include "utils/media/externaltimesource.h"
#include "utils/common/util.h"

static const qint64 SYNC_EPS = 1000 * 1000;

struct ReaderInfo
{
    ReaderInfo(QnAbstractArchiveReader* _reader, QnAbstractArchiveDelegate* _oldDelegate, QnlTimeSource* _cam):
        reader(_reader),
        oldDelegate(_oldDelegate),
        cam(_cam),
        enabled(true)
    {
    }
    QnAbstractArchiveReader* reader;
    QnAbstractArchiveDelegate* oldDelegate;
    QnlTimeSource* cam;
    bool enabled;
};

class QnArchiveSyncPlayWrapper::QnArchiveSyncPlayWrapperPrivate
{
public:
    QnArchiveSyncPlayWrapperPrivate()
    {
        blockSingleShotSignal = false;
        blockJumpSignal = false;
        blockPausePlaySignal = false;
        lastJumpTime = DATETIME_NOW;
    }

    QList<ReaderInfo> readers;
    QMutex mutex;

    QWaitCondition syncCond;
    mutable QMutex syncMutex;

    bool blockSingleShotSignal;
    bool blockJumpSignal;
    bool blockPausePlaySignal;
    qint64 lastJumpTime;
};

// ------------------- QnArchiveSyncPlayWrapper ----------------------------

QnArchiveSyncPlayWrapper::QnArchiveSyncPlayWrapper():
    d_ptr(new QnArchiveSyncPlayWrapperPrivate())
{
}

QnArchiveSyncPlayWrapper::~QnArchiveSyncPlayWrapper()
{

    delete d_ptr;
}

void QnArchiveSyncPlayWrapper::addArchiveReader(QnAbstractArchiveReader* reader, QnlTimeSource* cam)
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (reader == 0)
        return;
    d->readers << ReaderInfo(reader, reader->getArchiveDelegate(), cam);
    

    reader->setArchiveDelegate(new QnSyncPlayArchiveDelegate(reader, this, reader->getArchiveDelegate()));
    reader->setCycleMode(false);

    connect(reader, SIGNAL(singleShotModeChanged(bool)), this, SLOT(onSingleShotModeChanged(bool)), Qt::DirectConnection);
    connect(reader, SIGNAL(beforeJump(qint64, bool)), this, SLOT(onBeforeJump(qint64, bool)), Qt::DirectConnection);
    connect(reader, SIGNAL(jumpOccured(qint64)), this, SLOT(onJumpOccured(qint64)), Qt::DirectConnection);

    connect(reader, SIGNAL(streamPaused()), this, SLOT(onStreamPaused()), Qt::DirectConnection);
    connect(reader, SIGNAL(streamResumed()), this, SLOT(onStreamResumed()), Qt::DirectConnection);
    connect(reader, SIGNAL(nextFrameOccured()), this, SLOT(onNextPrevFrameOccured()), Qt::DirectConnection);
    connect(reader, SIGNAL(prevFrameOccured()), this, SLOT(onNextPrevFrameOccured()), Qt::DirectConnection);

    connect(reader, SIGNAL(speedChanged(double)), this, SLOT(onSpeedChanged(double)), Qt::DirectConnection);
}

void QnArchiveSyncPlayWrapper::onNextPrevFrameOccured()
{
    Q_D(QnArchiveSyncPlayWrapper);
    foreach(const ReaderInfo& info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        syncDelegate->enableSync(info.enabled);
    }
}

void QnArchiveSyncPlayWrapper::onStreamResumed()
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (d->blockPausePlaySignal)
        return;
    d->blockPausePlaySignal = true;
    foreach(ReaderInfo info, d->readers)
    {
        if (info.reader != sender())
            info.reader->resumeMedia();
    }
    d->blockPausePlaySignal = false;
}

void QnArchiveSyncPlayWrapper::onSpeedChanged(double value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (d->blockPausePlaySignal)
        return;
    d->blockPausePlaySignal = true;
    foreach(ReaderInfo info, d->readers)
    {
        if (info.reader != sender())
            info.reader->setSpeed(value);
    }
    d->blockPausePlaySignal = false;
}

void QnArchiveSyncPlayWrapper::onStreamPaused()
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (d->blockPausePlaySignal)
        return;
    d->blockPausePlaySignal = true;
    foreach(ReaderInfo info, d->readers)
    {
        if (info.reader != sender())
            info.reader->pause();
    }
    d->blockPausePlaySignal = false;
}

void QnArchiveSyncPlayWrapper::onSingleShotModeChanged(bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (d->blockSingleShotSignal)
        return;
    d->blockSingleShotSignal = true;
    foreach(const ReaderInfo& info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        syncDelegate->enableSync(!value && info.enabled);

        if (info.reader != sender())
            info.reader->setSingleShotMode(value);
    }
    d->blockSingleShotSignal = false;
    d->syncCond.wakeAll();
}

void QnArchiveSyncPlayWrapper::onBeforeJump(qint64 mksec, bool makeshot)
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (d->blockJumpSignal)
        return;
    d->blockJumpSignal = true;
    d->lastJumpTime = mksec;
    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        syncDelegate->enableSync(false);
        if (info.reader != sender() && info.enabled)
        {
            syncDelegate->jumpToPreviousFrame(mksec, makeshot);
        }
    }
    d->syncCond.wakeAll();
    d->blockJumpSignal = false;
}

void QnArchiveSyncPlayWrapper::onJumpOccured(qint64 mksec)
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (d->blockJumpSignal)
        return;
    d->blockJumpSignal = true;

    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        syncDelegate->enableSync(info.enabled && !info.reader->isSingleShotMode());
    }
    d->blockJumpSignal = false;
    d->syncCond.wakeAll();
}

qint64 QnArchiveSyncPlayWrapper::minTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    qint64 result = 0x7fffffffffffffffULL;
    foreach(ReaderInfo info, d->readers)
        result = qMin(result, info.oldDelegate->startTime());
    return result;
}

qint64 QnArchiveSyncPlayWrapper::endTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    qint64 result = 0;
    foreach(ReaderInfo info, d->readers)
        result = qMax(result, info.oldDelegate->endTime());
    return result;
}

/*
qint64 QnArchiveSyncPlayWrapper::seek (qint64 time)
{
    Q_D(QnArchiveSyncPlayWrapper);
    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        syncDelegate->jumpTo(time, false);
    }
    d->syncCond.wakeAll();
    return time;
}
*/

qint64 QnArchiveSyncPlayWrapper::secondTime(QnAbstractArchiveReader* reader) const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    qint64 rez = 0x7fffffffffffffffll;
    foreach(ReaderInfo info, d->readers)
    {
        if (!info.enabled || info.reader == reader) // skip self and disabled
            continue;
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        qint64 val = syncDelegate->secondTime();
        if (val != AV_NOPTS_VALUE)
            rez = qMin(rez, val);
    }
    return rez;
}

void QnArchiveSyncPlayWrapper::waitIfNeed(QnAbstractArchiveReader* reader, qint64 timestamp)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->syncMutex);
    // +SYNC_EPS
    while (!reader->needToStop() && timestamp > secondTime(reader)) // time of second(next) frame is less than it time.
    {
        //QString msg;
        //QTextStream str(&msg);
        //str << "waiting. curTime=" << QDateTime::fromMSecsSinceEpoch(timestamp/1000).toString("hh:mm:ss.zzz") << 
        //       " secondtime=" << QDateTime::fromMSecsSinceEpoch(secondTime()/1000).toString("hh:mm:ss.zzz");
        //str.flush();
        //cl_log.log(msg, cl_logWARNING);

        d->syncCond.wait(&d->syncMutex, 50);
    }
}

void QnArchiveSyncPlayWrapper::onNewDataReaded()
{
    Q_D(QnArchiveSyncPlayWrapper);
    d->syncCond.wakeAll();
}

void QnArchiveSyncPlayWrapper::removeArchiveReader(QnAbstractArchiveReader* reader)
{
    reader->disconnect(this);
    erase(reader->getArchiveDelegate());
}

void QnArchiveSyncPlayWrapper::erase(QnAbstractArchiveDelegate* value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->syncMutex);
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->reader->getArchiveDelegate() == value)
        {
            d->readers.erase(i);
            break;
        }
    }
}

qint64 QnArchiveSyncPlayWrapper::selfCurrentTime(void) const
{
    return getCurrentTime();
}


qint64 QnArchiveSyncPlayWrapper::getCurrentTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->syncMutex);
    qint64 val = AV_NOPTS_VALUE;
    foreach(ReaderInfo info, d->readers)
    {
        if (!info.enabled) 
            continue;
        qint64 camTime = info.cam->selfCurrentTime();
        if (camTime != AV_NOPTS_VALUE)
            val = qMax(camTime, val);
    }
    return val;
}

void QnArchiveSyncPlayWrapper::onConsumerBlocksReader(QnAbstractStreamDataProvider* reader, bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    for (int i = 0; i < d->readers.size(); ++i)
    {
        if (d->readers[i].reader == reader) 
        {
            QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (d->readers[i].reader->getArchiveDelegate());
            if (!d->readers[i].enabled && !value)
            {
                qint64 time = getCurrentTime();
                d->blockJumpSignal = true;
                //d->readers[i].reader->setSkipFramesToTime(time);
                if (d->lastJumpTime != DATETIME_NOW)
                    syncDelegate->jumpToPreviousFrame(time, true);
                d->blockJumpSignal = false;
                // resume display
            }
            d->readers[i].enabled = !value;
            if (!d->readers[i].enabled)
                syncDelegate->enableSync(false);
            d->syncCond.wakeAll();
        }
    }
}
