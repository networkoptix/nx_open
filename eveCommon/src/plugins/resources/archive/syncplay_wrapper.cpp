#include <QWaitCondition>

#include "syncplay_wrapper.h"
#include "syncplay_archive_delegate.h"
#include "abstract_archive_stream_reader.h"

static const qint64 WAIT_EPS = 0; //100 * 1000;

struct ReaderInfo
{
    ReaderInfo(QnAbstractArchiveReader* _reader, QnAbstractArchiveDelegate* _oldDelegate, CLVideoCamera* _cam):
        reader(_reader),
        oldDelegate(_oldDelegate),
        cam(_cam)
    {
    }
    QnAbstractArchiveReader* reader;
    QnAbstractArchiveDelegate* oldDelegate;
    CLVideoCamera* cam;
};

struct QnArchiveSyncPlayWrapper::QnArchiveSyncPlayWrapperPrivate
{
    QnArchiveSyncPlayWrapperPrivate()
    {
        blockSingleShotSignal = false;
        blockJumpSignal = false;
        blockPausePlaySignal = false;
    }

    QList<ReaderInfo> readers;
    QMutex mutex;

    QWaitCondition syncCond;
    mutable QMutex syncMutex;

    bool blockSingleShotSignal;
    bool blockJumpSignal;
    bool blockPausePlaySignal;
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

void QnArchiveSyncPlayWrapper::addArchiveReader(QnAbstractArchiveReader* reader, CLVideoCamera* cam)
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (reader == 0)
        return;
    d->readers << ReaderInfo(reader, reader->getArchiveDelegate(), cam);
    cam->setExternalTimeSource(this);

    reader->setArchiveDelegate(new QnSyncPlayArchiveDelegate(reader, this, reader->getArchiveDelegate()));
    reader->setCycleMode(false);

    connect(reader, SIGNAL(singleShotModeChanged(bool)), this, SLOT(onSingleShotModeChanged(bool)), Qt::DirectConnection);
    connect(reader, SIGNAL(jumpOccured(qint64, bool)), this, SLOT(onJumpOccured(qint64, bool)), Qt::DirectConnection);

    connect(reader, SIGNAL(streamPaused()), this, SLOT(onStreamPaused()), Qt::DirectConnection);
    connect(reader, SIGNAL(streamResumed()), this, SLOT(onStreamResumed()), Qt::DirectConnection);
    connect(reader, SIGNAL(nextFrameOccured()), this, SLOT(onNextPrevFrameOccured()), Qt::DirectConnection);
    connect(reader, SIGNAL(prevFrameOccured()), this, SLOT(onNextPrevFrameOccured()), Qt::DirectConnection);
}

void QnArchiveSyncPlayWrapper::onNextPrevFrameOccured()
{
    Q_D(QnArchiveSyncPlayWrapper);
    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        syncDelegate->enableSync(true);
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
            info.reader->resume();
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
    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        syncDelegate->enableSync(!value);

        if (info.reader != sender())
            info.reader->setSingleShotMode(value);
    }
    d->blockSingleShotSignal = false;
    d->syncCond.wakeAll();
}

void QnArchiveSyncPlayWrapper::onJumpOccured(qint64 mksec, bool makeshot)
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (d->blockJumpSignal)
        return;
    d->blockJumpSignal = true;

    qint64 skipFramesToTime = 0;
    foreach(ReaderInfo info, d->readers)
    {
        if (info.reader == sender())
        {
            skipFramesToTime = info.reader->skipFramesToTime();
            break;
        }
    }

    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        if (info.reader->isSingleShotMode())
            syncDelegate->enableSync(false);
        if (info.reader != sender())
        {
            info.reader->setSkipFramesToTime(skipFramesToTime);
            syncDelegate->jumpTo(mksec, makeshot);
        }
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

qint64 QnArchiveSyncPlayWrapper::secondTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    qint64 rez = 0x7fffffffffffffffll;
    foreach(ReaderInfo info, d->readers)
    {
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
    while (!reader->needToStop() && timestamp > secondTime()+WAIT_EPS) // time of second(next) frame is less than it time.
    {
        d->syncCond.wait(&d->syncMutex, 10);
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

qint64 QnArchiveSyncPlayWrapper::getCurrentTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->syncMutex);
    qint64 val = 0;
    foreach(ReaderInfo info, d->readers)
    {
        val = qMax(info.cam->selfCurrentTime(), 0ll);
    }
    return val;
}
