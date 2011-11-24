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
    void initValues()
    {
        blockSingleShotSignal = false;
        blockJumpSignal = false;
        blockPausePlaySignal = false;
        lastJumpTime = DATETIME_NOW;
        maxAllowedDate.clear();
        minAllowedDate.clear();
        speed = 1.0;
    }


    QnArchiveSyncPlayWrapperPrivate():
      timeMutex(QMutex::Recursive)
    {
        initValues();
    }

    QList<ReaderInfo> readers;
    mutable QMutex timeMutex;

    QWaitCondition syncCond;
    mutable QMutex syncMutex;

    bool blockSingleShotSignal;
    bool blockJumpSignal;
    bool blockPausePlaySignal;
    qint64 lastJumpTime;
    QTime timer;
    double speed;
    QMap<QnlTimeSource*,qint64> maxAllowedDate;
    QMap<QnlTimeSource*,qint64> minAllowedDate;
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
    

    reader->setArchiveDelegate(new QnSyncPlayArchiveDelegate(reader, this, reader->getArchiveDelegate(), cam));
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
    d->speed = value;

    reinitTime();

    d->blockPausePlaySignal = false;
}

qint64 QnArchiveSyncPlayWrapper::getDisplayedTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    qint64 displayTime = AV_NOPTS_VALUE;
    foreach(ReaderInfo info, d->readers)
    {
        qint64 time = info.cam->getCurrentTime();
        if (displayTime == AV_NOPTS_VALUE)
            displayTime = time;
        else if (time != AV_NOPTS_VALUE)
            displayTime = d->speed >= 0 ? qMin(time, displayTime) : qMax(time, displayTime);
    }
    return displayTime;
}

void QnArchiveSyncPlayWrapper::reinitTime()
{
    Q_D(QnArchiveSyncPlayWrapper);

    // todo: add negative speed support here
    qint64 displayTime = getDisplayedTime();
    if (displayTime != AV_NOPTS_VALUE)
        d->lastJumpTime = displayTime;
    else
        d->lastJumpTime = getCurrentTime();
    d->timer.restart();
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

    {
        QMutexLocker lock(&d->timeMutex);
        d->lastJumpTime = mksec;
        cl_log.log("delegateJump=", QDateTime::fromMSecsSinceEpoch(mksec/1000).toString("hh:mm:ss.zzz"), cl_logALWAYS);
        d->maxAllowedDate.clear();
        d->minAllowedDate.clear();
        d->timer.restart();
    }    
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
    
    qint64 result = INT64_MAX;
    bool found = false;
    foreach(ReaderInfo info, d->readers) {
        qint64 startTime = info.oldDelegate->startTime();
        if(startTime != AV_NOPTS_VALUE) {
            result = qMin(result, startTime);
            found = true;
        }
    }
    return found ? result : AV_NOPTS_VALUE;
}

qint64 QnArchiveSyncPlayWrapper::endTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);

    qint64 result = 0;
    bool found = false;
    foreach(ReaderInfo info, d->readers) {
        qint64 endTime = info.oldDelegate->endTime();
        if(endTime != AV_NOPTS_VALUE) {
            result = qMax(result, endTime);
            found = true;
        }
    }
    return found ? result : AV_NOPTS_VALUE;
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

qint64 QnArchiveSyncPlayWrapper::getNextTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    qint64 rez = AV_NOPTS_VALUE;
    foreach(ReaderInfo info, d->readers)
    {
        if (!info.enabled) // skip self and disabled
            continue;
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        qint64 val = syncDelegate->secondTime();
        if (rez == AV_NOPTS_VALUE)
            rez = val;
        else if (val != AV_NOPTS_VALUE)
            rez = qMin(rez, val);
    }
    return rez;
}

/*
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
*/

/*
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
*/

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
            if (d->readers.isEmpty())
                d->initValues();
            break;
        }
    }
}

void QnArchiveSyncPlayWrapper::onAvailableTime(QnlTimeSource* source, qint64 time)
{
    Q_D(QnArchiveSyncPlayWrapper);

    if (time != AV_NOPTS_VALUE)
    {
        QMutexLocker lock(&d->timeMutex);
        if (d->speed >= 0)
        {
            QMap<QnlTimeSource*, qint64>::iterator itr = d->maxAllowedDate.find(source);
            if (itr == d->maxAllowedDate.end())
                d->maxAllowedDate.insert(source, time);
            else
                *itr = qMax(*itr, time);
        }
        else {
            QMap<QnlTimeSource*, qint64>::iterator itr = d->minAllowedDate.find(source);
            if (itr == d->minAllowedDate.end())
                d->minAllowedDate.insert(source, time);
            else
                *itr = qMin(*itr, time);
        }
    }
}


qint64 QnArchiveSyncPlayWrapper::getCurrentTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    if (d->lastJumpTime == DATETIME_NOW)
        return d->lastJumpTime;
    qint64 expectedTime = d->lastJumpTime + d->timer.elapsed()*1000 * d->speed;
    if (d->speed >= 0) 
    {
        qint64 maxAllowedDate = INT64_MAX;
        foreach(qint64 val, d->maxAllowedDate.values()) 
            maxAllowedDate = qMin(maxAllowedDate, val);
        
        return qMin(expectedTime, maxAllowedDate);
    }
    else {
        qint64 minAllowedDate = 0;
        foreach(qint64 val, d->minAllowedDate.values()) 
            minAllowedDate = qMax(minAllowedDate, val);

        return qMax(expectedTime, minAllowedDate);
    }
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
