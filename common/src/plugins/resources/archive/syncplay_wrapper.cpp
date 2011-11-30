#include <QWaitCondition>

#include "syncplay_wrapper.h"
#include "syncplay_archive_delegate.h"
#include "abstract_archive_stream_reader.h"
#include "utils/media/externaltimesource.h"
#include "utils/common/util.h"

static const qint64 SYNC_EPS = 1000 * 500;
static const qint64 SYNC_FOR_FRAME_EPS = 1000 * 50;

struct ReaderInfo
{
    ReaderInfo(QnAbstractArchiveReader* _reader, QnAbstractArchiveDelegate* _oldDelegate, QnlTimeSource* _cam):
        reader(_reader),
        oldDelegate(_oldDelegate),
        cam(_cam),
        enabled(true),
        buffering(false)
    {
    }
    QnAbstractArchiveReader* reader;
    QnAbstractArchiveDelegate* oldDelegate;
    QnlTimeSource* cam;
    bool enabled;
    bool buffering;
};


class QnArchiveSyncPlayWrapper::QnArchiveSyncPlayWrapperPrivate
{
public:
    void initValues()
    {
        blockSetSpeedSignal = false;
        lastJumpTime = DATETIME_NOW;
        inJumpCount = 0;
        speed = 1.0;
    }


    QnArchiveSyncPlayWrapperPrivate():
      timeMutex(QMutex::Recursive)
    {
        initValues();
    }

    QList<ReaderInfo> readers;
    mutable QMutex timeMutex;

    bool blockSetSpeedSignal;
    qint64 lastJumpTime;
    int inJumpCount;
    QTime timer;
    double speed;
    //QMap<QnlTimeSource*,qint64> maxAllowedDate;
    //QMap<QnlTimeSource*,qint64> minAllowedDate;
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

// ----------------- Navigation ----------------------------

void QnArchiveSyncPlayWrapper::resumeMedia()
{
    Q_D(QnArchiveSyncPlayWrapper);
    reinitTime(getDisplayedTime());
    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        //syncDelegate->setPrebuffering(false);
        info.reader->setNavDelegate(0);
        info.reader->resumeMedia();
        info.reader->setNavDelegate(this);
    }
}

void QnArchiveSyncPlayWrapper::pauseMedia()
{
    Q_D(QnArchiveSyncPlayWrapper);
    foreach(ReaderInfo info, d->readers)
    {
        info.reader->setNavDelegate(0);
        info.reader->pauseMedia();
        info.reader->setNavDelegate(this);
    }
}

void QnArchiveSyncPlayWrapper::setSingleShotMode(bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    foreach(const ReaderInfo& info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        info.reader->setNavDelegate(0);
        info.reader->setSingleShotMode(value);
        info.reader->setNavDelegate(this);
    }
    if (!value)
        reinitTime(getDisplayedTime());
}

bool QnArchiveSyncPlayWrapper::jumpTo(qint64 mksec,  qint64 skipTime)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    d->lastJumpTime = skipTime ? skipTime : mksec;
    d->inJumpCount = 0;
    d->timer.restart();
    bool rez = false;
    foreach(ReaderInfo info, d->readers)
    {
        if (info.enabled)
        {
            QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
            //syncDelegate->setPrebuffering(false);
            info.reader->setNavDelegate(0);
            rez |= info.reader->jumpTo(mksec, skipTime);
            info.reader->setNavDelegate(this);
        }
    }
    return rez;
}

void QnArchiveSyncPlayWrapper::nextFrame()
{
    Q_D(QnArchiveSyncPlayWrapper);
    qint64 mintTime = AV_NOPTS_VALUE;
    foreach(const ReaderInfo& info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        if (info.enabled) {
            //syncDelegate->setPrebuffering(true);
            qint64 curTime = info.cam->getCurrentTime();
            if (mintTime == AV_NOPTS_VALUE)
                mintTime = curTime;
            else if (curTime != AV_NOPTS_VALUE)
                mintTime = qMin(mintTime, curTime);
        }
    }
    foreach(const ReaderInfo& info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());

        if (info.enabled) 
        {
            if (mintTime == AV_NOPTS_VALUE || info.cam->getCurrentTime() <= mintTime+SYNC_FOR_FRAME_EPS)
            {
                info.reader->setNavDelegate(0);
                info.reader->nextFrame();
                info.reader->setNavDelegate(this);
            }
        }
    }
}

void QnArchiveSyncPlayWrapper::previousFrame(qint64 mksec)
{
    Q_D(QnArchiveSyncPlayWrapper);
    foreach(const ReaderInfo& info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        //syncDelegate->setPrebuffering(false);
        info.reader->setNavDelegate(0);
        info.reader->previousFrame(mksec);
        info.reader->setNavDelegate(this);
    }
}


// -------------------------- end of navigation ----------------------------

void QnArchiveSyncPlayWrapper::addArchiveReader(QnAbstractArchiveReader* reader, QnlTimeSource* cam)
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (reader == 0)
        return;

    reader->setNavDelegate(this);

    d->readers << ReaderInfo(reader, reader->getArchiveDelegate(), cam);
    

    reader->setArchiveDelegate(new QnSyncPlayArchiveDelegate(reader, this, reader->getArchiveDelegate()));
    reader->setCycleMode(false);

    connect(reader, SIGNAL(beforeJump(qint64)), this, SLOT(onBeforeJump(qint64)), Qt::DirectConnection);
    connect(reader, SIGNAL(jumpOccured(qint64)), this, SLOT(onJumpOccured(qint64)), Qt::DirectConnection);

    connect(reader, SIGNAL(speedChanged(double)), this, SLOT(onSpeedChanged(double)), Qt::DirectConnection);

    //connect(reader, SIGNAL(singleShotModeChanged(bool)), this, SLOT(onSingleShotModeChanged(bool)), Qt::DirectConnection);
    //connect(reader, SIGNAL(streamPaused()), this, SLOT(onStreamPaused()), Qt::DirectConnection);
    //connect(reader, SIGNAL(streamResumed()), this, SLOT(onStreamResumed()), Qt::DirectConnection);
    //connect(reader, SIGNAL(nextFrameOccured()), this, SLOT(onNextFrameOccured()), Qt::DirectConnection);
    //connect(reader, SIGNAL(prevFrameOccured()), this, SLOT(onPrevFrameOccured()), Qt::DirectConnection);
}

void QnArchiveSyncPlayWrapper::onSpeedChanged(double value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    if (d->blockSetSpeedSignal)
        return;
    d->blockSetSpeedSignal = true;
    foreach(ReaderInfo info, d->readers)
    {
        QnSyncPlayArchiveDelegate* syncDelegate = static_cast<QnSyncPlayArchiveDelegate*> (info.reader->getArchiveDelegate());
        //syncDelegate->setPrebuffering(false);
        if (info.reader != sender())
            info.reader->setSpeed(value);
    }
    reinitTime(getDisplayedTimeInternal());
    d->speed = value;

    d->blockSetSpeedSignal = false;
}

qint64 QnArchiveSyncPlayWrapper::getNextTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    qint64 displayTime = AV_NOPTS_VALUE;
    foreach(ReaderInfo info, d->readers)
    {
        if (info.enabled) {
            qint64 time = info.cam->getNextTime();
            if (displayTime == AV_NOPTS_VALUE)
                displayTime = time;
            else if (time != AV_NOPTS_VALUE)
                displayTime = d->speed >= 0 ? qMin(time, displayTime) : qMax(time, displayTime);
        }
    }
    return displayTime;
}


qint64 QnArchiveSyncPlayWrapper::getDisplayedTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);

    if (d->lastJumpTime == DATETIME_NOW)
        return DATETIME_NOW;

    return getDisplayedTimeInternal();
}

qint64 QnArchiveSyncPlayWrapper::getDisplayedTimeInternal() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);

    qint64 displayTime = AV_NOPTS_VALUE;
    foreach(ReaderInfo info, d->readers)
    {
        if (info.enabled) {
            qint64 time = info.cam->getCurrentTime();
            if (displayTime == AV_NOPTS_VALUE)
                displayTime = time;
            else if (time != AV_NOPTS_VALUE)
                displayTime = d->speed >= 0 ? qMin(time, displayTime) : qMax(time, displayTime);
        }
    }
    return displayTime;
}

void QnArchiveSyncPlayWrapper::reinitTime(qint64 newTime)
{
    Q_D(QnArchiveSyncPlayWrapper);

    // todo: add negative speed support here
    if (newTime != AV_NOPTS_VALUE)
        d->lastJumpTime = newTime;
    else
        d->lastJumpTime = getCurrentTime();
    d->timer.restart();
}

void QnArchiveSyncPlayWrapper::onBeforeJump(qint64 mksec)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    d->inJumpCount++;
}


void QnArchiveSyncPlayWrapper::onJumpOccured(qint64 mksec)
{
    Q_D(QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    d->inJumpCount--;
    if (d->inJumpCount == 0)
        d->timer.restart();
}

qint64 QnArchiveSyncPlayWrapper::minTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    
    qint64 result = INT64_MAX;
    bool found = false;
    foreach(ReaderInfo info, d->readers) 
    {
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
    foreach(ReaderInfo info, d->readers) 
    {
        qint64 endTime = info.oldDelegate->endTime();
        if(endTime != AV_NOPTS_VALUE) {
            result = qMax(result, endTime);
            found = true;
        }
    }
    return found ? result : AV_NOPTS_VALUE;
}

void QnArchiveSyncPlayWrapper::removeArchiveReader(QnAbstractArchiveReader* reader)
{
    reader->disconnect(this);
    erase(reader->getArchiveDelegate());
}

void QnArchiveSyncPlayWrapper::erase(QnAbstractArchiveDelegate* value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
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

void QnArchiveSyncPlayWrapper::onBufferingStarted(QnlTimeSource* source)
{
    Q_D(QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->cam == source)
        {
            i->buffering = true;
            break;
        }
    }
}

void QnArchiveSyncPlayWrapper::onBufferingFinished(QnlTimeSource* source)
{
    Q_D(QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->cam == source)
        {
            i->buffering = false;
            d->timer.restart();
            break;
        }
    }
}

qint64 QnArchiveSyncPlayWrapper::getCurrentTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    if (d->lastJumpTime == DATETIME_NOW)
        return DATETIME_NOW;

    if (d->inJumpCount > 0)
        return d->lastJumpTime;

    foreach(const ReaderInfo& info, d->readers) {
        if (info.enabled && info.buffering) 
            return d->lastJumpTime;
    }

    qint64 expectedTime = d->lastJumpTime + d->timer.elapsed()*1000 * d->speed;

    /*
    QString s;
    QTextStream str(&s);
    str << "expectedTime=" << QDateTime::fromMSecsSinceEpoch(expectedTime/1000).toString("hh:mm:ss.zzz");
    str.flush();
    cl_log.log(s, cl_logALWAYS);
    */


    qint64 nextTime = getNextTime();
    if (d->speed >= 0 && nextTime != AV_NOPTS_VALUE && nextTime > expectedTime + MAX_FRAME_DURATION*1000)
    {
        QnArchiveSyncPlayWrapper* nonConstThis = const_cast<QnArchiveSyncPlayWrapper*>(this);
        nonConstThis->reinitTime(nextTime);
        expectedTime = d->lastJumpTime + d->timer.elapsed()*1000 * d->speed;
    }
    else if (d->speed < 0 && nextTime != AV_NOPTS_VALUE && nextTime < expectedTime - MAX_FRAME_DURATION*1000)
    {
        QnArchiveSyncPlayWrapper* nonConstThis = const_cast<QnArchiveSyncPlayWrapper*>(this);
        nonConstThis->reinitTime(nextTime);
        expectedTime = d->lastJumpTime + d->timer.elapsed()*1000 * d->speed;
        /*
        QString s;
        QTextStream str(&s);
        str << "reinitTimeTo=" << QDateTime::fromMSecsSinceEpoch(nextTime/1000).toString("hh:mm:ss.zzz") << "correctExpectedTime to=" << QDateTime::fromMSecsSinceEpoch(expectedTime/1000).toString("hh:mm:ss.zzz");
        str.flush();
        cl_log.log(s, cl_logALWAYS);
        */
    }

    qint64 displayedTime =  getDisplayedTimeInternal();
    if (displayedTime == AV_NOPTS_VALUE)
        return displayedTime;
    if (d->speed >= 0) 
        return qMin(expectedTime, displayedTime + SYNC_EPS);
    else 
        return qMax(expectedTime, displayedTime - SYNC_EPS);
}

void QnArchiveSyncPlayWrapper::onConsumerBlocksReader(QnAbstractStreamDataProvider* reader, bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    for (int i = 0; i < d->readers.size(); ++i)
    {
        if (d->readers[i].reader == reader) 
        {
            if (!d->readers[i].enabled && !value && d->lastJumpTime != DATETIME_NOW)
            {
                d->readers[i].reader->setNavDelegate(0);
                d->readers[i].reader->jumpToPreviousFrame(getCurrentTime());
                d->readers[i].reader->setNavDelegate(this);
            }
            d->readers[i].enabled = !value;
            break;
        }
    }
}
