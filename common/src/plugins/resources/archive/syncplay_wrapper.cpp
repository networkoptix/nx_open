#include <QWaitCondition>

#include "syncplay_wrapper.h"
#include "syncplay_archive_delegate.h"
#include "abstract_archive_stream_reader.h"
#include "utils/media/externaltimesource.h"
#include "utils/common/util.h"
#include "playbackmask_helper.h"
#include "utils/common/synctime.h"

static const qint64 SYNC_EPS = 1000 * 500;
static const qint64 SYNC_FOR_FRAME_EPS = 1000 * 50;

struct ReaderInfo
{
    ReaderInfo(QnAbstractArchiveReader* _reader, QnAbstractArchiveDelegate* _oldDelegate, QnlTimeSource* _cam):
        reader(_reader),
        oldDelegate(_oldDelegate),
        cam(_cam),
        enabled(true),
        buffering(false),
        isEOF(false),
        paused(false)
    {
    }
    QnAbstractArchiveReader* reader;
    QnAbstractArchiveDelegate* oldDelegate;
    QnlTimeSource* cam;
    bool enabled;
    bool buffering;
    bool isEOF;
    bool paused;
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
        processingJump = false;
        enabled = true;
        bufferingTime = AV_NOPTS_VALUE;
        paused = false;
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
    
    bool processingJump;
    bool enabled;
    qint64 bufferingTime;
    bool paused;
    //QnPlaybackMaskHelper playbackMaskHelper;
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
    QMutexLocker lock(&d->timeMutex);
    d->paused = false;
    qint64 time = getDisplayedTime();
    bool resumed = false;
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.reader->isMediaPaused())
        {
            info.reader->setNavDelegate(0);
            info.reader->resumeMedia();
            info.reader->setNavDelegate(this);
            resumed = true;
        }
    }
    if (resumed)
        reinitTime(time);
}

/*
bool QnArchiveSyncPlayWrapper::setSendMotion(bool value)
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    bool rez = true;
    foreach(ReaderInfo info, d->readers) {
        info.reader->setNavDelegate(0);
        rez &= info.reader->setSendMotion(value);
        info.reader->setNavDelegate(this);
    }
    return rez;
}
*/

/*
bool QnArchiveSyncPlayWrapper::setMotionRegion(const QRegion& region)
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    bool rez = true;
    foreach(ReaderInfo info, d->readers) {
        info.reader->setNavDelegate(0);
        rez &= info.reader->setMotionRegion(region);
        info.reader->setNavDelegate(this);
    }
    return rez;
}
*/

bool QnArchiveSyncPlayWrapper::isMediaPaused() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    bool rez = true;
    foreach(const ReaderInfo& info, d->readers)
        rez &= info.reader->isMediaPaused();
    return rez;
}

void QnArchiveSyncPlayWrapper::pauseMedia()
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    d->paused = true;
    foreach(const ReaderInfo& info, d->readers)
    {
        info.reader->setNavDelegate(0);
        info.reader->pauseMedia();
        info.reader->setNavDelegate(this);
    }
}

void QnArchiveSyncPlayWrapper::setSingleShotMode(bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    foreach(const ReaderInfo& info, d->readers)
    {
        info.reader->setNavDelegate(0);
        info.reader->setSingleShotMode(value);
        info.reader->setNavDelegate(this);
    }
    if (!value)
        reinitTime(getDisplayedTime());
}

void QnArchiveSyncPlayWrapper::directJumpToNonKeyFrame(qint64 mksec)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    d->lastJumpTime = mksec;
    d->timer.restart();
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.enabled)
        {
            info.reader->setNavDelegate(0);
            info.reader->directJumpToNonKeyFrame(mksec);
            info.reader->setNavDelegate(this);
            d->processingJump = true;
        }
    }
}

void QnArchiveSyncPlayWrapper::setJumpTime(qint64 mksec)
{
    Q_D(QnArchiveSyncPlayWrapper);
    d->lastJumpTime = mksec;
    if (d->speed < 0 && d->lastJumpTime == DATETIME_NOW)
        d->lastJumpTime = qnSyncTime->currentMSecsSinceEpoch()*1000ll; // keep camera sync after jump to live position (really in reverse mode cameras stay in archive)
    d->timer.restart();
}

bool QnArchiveSyncPlayWrapper::jumpTo(qint64 mksec,  qint64 skipTime)
{
    qDebug() << "jumpTo=" << QDateTime::fromMSecsSinceEpoch(mksec).toString();

    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    setJumpTime(skipTime ? skipTime : mksec);
    bool rez = false;
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.enabled)
        {
            info.reader->setNavDelegate(0);
            rez |= info.reader->jumpTo(mksec, skipTime);
            info.reader->setNavDelegate(this);
        }
    }
    d->processingJump = rez;
    return rez;
}

void QnArchiveSyncPlayWrapper::nextFrame()
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    qint64 mintTime = AV_NOPTS_VALUE;
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.enabled) {
            qint64 curTime = info.cam->getCurrentTime();
            if (mintTime == AV_NOPTS_VALUE)
                mintTime = curTime;
            else if (curTime != AV_NOPTS_VALUE)
                mintTime = qMin(mintTime, curTime);
        }
    }
    foreach(const ReaderInfo& info, d->readers)
    {
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
    QMutexLocker lock(&d->timeMutex);
    foreach(const ReaderInfo& info, d->readers)
    {
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

    qint64 currentTime = getDisplayedTime();

    QMutexLocker lock(&d->timeMutex);

    d->readers << ReaderInfo(reader, reader->getArchiveDelegate(), cam);
    
    reader->setArchiveDelegate(new QnSyncPlayArchiveDelegate(reader, this, reader->getArchiveDelegate()));
    reader->setCycleMode(false);

    connect(reader, SIGNAL(beforeJump(qint64)), this, SLOT(onBeforeJump(qint64)), Qt::DirectConnection);
    connect(reader, SIGNAL(jumpOccured(qint64)), this, SLOT(onJumpOccured(qint64)), Qt::DirectConnection);
    connect(reader, SIGNAL(jumpCanceled(qint64)), this, SLOT(onJumpCanceled(qint64)), Qt::DirectConnection);

    if (d->enabled && currentTime != DATETIME_NOW) {
        reader->jumpToPreviousFrame(currentTime);
        reader->setSpeed(d->speed, currentTime);
        if (d->speed == 0)
            reader->pauseMedia();
    }

    if (d->enabled)
        reader->setNavDelegate(this);

    //connect(reader, SIGNAL(singleShotModeChanged(bool)), this, SLOT(onSingleShotModeChanged(bool)), Qt::DirectConnection);
    //connect(reader, SIGNAL(streamPaused()), this, SLOT(onStreamPaused()), Qt::DirectConnection);
    //connect(reader, SIGNAL(streamResumed()), this, SLOT(onStreamResumed()), Qt::DirectConnection);
    //connect(reader, SIGNAL(nextFrameOccured()), this, SLOT(onNextFrameOccured()), Qt::DirectConnection);
    //connect(reader, SIGNAL(prevFrameOccured()), this, SLOT(onPrevFrameOccured()), Qt::DirectConnection);
}

void QnArchiveSyncPlayWrapper::setSpeed(double value, qint64 /*currentTimeHint*/)
{
    Q_D(QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);

    if (value == d->speed)
        return;

    qint64 displayedTime = getDisplayedTimeInternal();
    qDebug() << "Speed changed. CurrentTime" << QDateTime::fromMSecsSinceEpoch(displayedTime/1000).toString();
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.enabled) {
            info.reader->setNavDelegate(0);
            info.reader->setSpeed(value, d->lastJumpTime == DATETIME_NOW ? DATETIME_NOW : displayedTime);
            info.reader->setNavDelegate(this);
        }
    }
    if (d->lastJumpTime == DATETIME_NOW || displayedTime == DATETIME_NOW)
        displayedTime = qnSyncTime->currentMSecsSinceEpoch()*1000;
    qint64 et = expectedTime();
    int sign = d->speed >= 0 ? 1 : -1;
    if (d->speed != 0 && value != 0 && sign*(et - displayedTime) > SYNC_EPS) 
        reinitTime(displayedTime);
    d->speed = value;
}

qint64 QnArchiveSyncPlayWrapper::getNextTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    qint64 displayTime = AV_NOPTS_VALUE;
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.enabled && !info.isEOF) {
            qint64 time = info.cam->getNextTime();
            if (displayTime == AV_NOPTS_VALUE)
                displayTime = time;
            else if (time != AV_NOPTS_VALUE)
                displayTime = d->speed >= 0 ? qMin(time, displayTime) : qMax(time, displayTime);
        }
    }
    return displayTime;
}

qint64 QnArchiveSyncPlayWrapper::getExternalTime() const
{
    return getDisplayedTime();
}

qint64 QnArchiveSyncPlayWrapper::getDisplayedTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);

    if (d->lastJumpTime == DATETIME_NOW && !d->paused)
        return DATETIME_NOW;

    return getDisplayedTimeInternal();
}

qint64 QnArchiveSyncPlayWrapper::getDisplayedTimeInternal() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);

    qint64 displayTime = AV_NOPTS_VALUE;
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.enabled && !info.isEOF) {
            qint64 time = info.cam->getCurrentTime();
            //if (time == DATETIME_NOW)
            //    time = qnSyncTime->currentMSecsSinceEpoch()*1000;
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

    if (newTime != AV_NOPTS_VALUE)
        d->lastJumpTime = newTime;
    else
        d->lastJumpTime = getCurrentTime();
		
    qDebug() << "reinitTime=" << QDateTime::fromMSecsSinceEpoch(d->lastJumpTime/1000).toString("hh:mm:ss.zzz");

    d->timer.restart();
}

void QnArchiveSyncPlayWrapper::onBeforeJump(qint64 /*mksec*/)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    d->inJumpCount++;
}

void QnArchiveSyncPlayWrapper::onJumpCanceled(qint64 /*time*/)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    d->inJumpCount--;
    Q_ASSERT(d->inJumpCount >= 0);
    if (d->inJumpCount == 0) 
    {
        d->processingJump = false;
    }
}

void QnArchiveSyncPlayWrapper::onJumpOccured(qint64 mksec)
{
    Q_D(QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    d->inJumpCount--;
    Q_ASSERT(d->inJumpCount >= 0);
    if (d->inJumpCount == 0) 
    {
        d->processingJump = false;
        setJumpTime(mksec);
    }
}

qint64 QnArchiveSyncPlayWrapper::minTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    
    qint64 result = INT64_MAX;
    bool found = false;
    foreach(const ReaderInfo& info, d->readers) 
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
    QMutexLocker lock(&d->timeMutex);

    qint64 result = 0;
    bool found = false;
    foreach(const ReaderInfo& info, d->readers) 
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
            i->reader->disconnect(this);
            d->readers.erase(i);
            if (d->readers.isEmpty())
                d->initValues();
            d->bufferingTime = AV_NOPTS_VALUE; // possible current item in buffering or jumping state
            break;
        }
    }
}

void QnArchiveSyncPlayWrapper::onBufferingStarted(QnlTimeSource* source, qint64 bufferingTime)
{
    Q_D(QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->cam == source)
        {
            i->buffering = true;
            d->bufferingTime = bufferingTime;
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
            break;
        }
    }


    //if (d->inJumpCount > 0)
    //    return;

    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->buffering && i->enabled)
            return;
    }

    // no more buffering
    qint64 bt = d->bufferingTime;
    d->bufferingTime = AV_NOPTS_VALUE;

    qDebug() << "correctTime after end of buffering=" << (getCurrentTime() - bt)/1000.0;
    reinitTime(getCurrentTime());
}

void QnArchiveSyncPlayWrapper::onEofReached(QnlTimeSource* source, bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->cam == source)
        {
            i->isEOF = value;
            break;
        }
    }

    bool allReady = d->speed > 0;
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->enabled)
            allReady &= i->isEOF;
    }

    if (allReady)
        jumpTo(DATETIME_NOW, 0);
}

qint64 QnArchiveSyncPlayWrapper::expectedTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    return d->lastJumpTime + d->timer.elapsed()*1000 * d->speed;
}

qint64 QnArchiveSyncPlayWrapper::getCurrentTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    if (d->lastJumpTime == DATETIME_NOW)
        return DATETIME_NOW;

    if (d->inJumpCount > 0)
        return d->lastJumpTime;

    if (d->bufferingTime != AV_NOPTS_VALUE)
        return d->bufferingTime; // same as last jump time

    foreach(const ReaderInfo& info, d->readers) {
        if (info.enabled && info.buffering) 
            return d->lastJumpTime;
    }

    qint64 expectTime = expectedTime();

    /*
    QString s;
    QTextStream str(&s);
    str << "expectTime=" << QDateTime::fromMSecsSinceEpoch(expectTime/1000).toString("hh:mm:ss.zzz");
    str.flush();
    cl_log.log(s, cl_logALWAYS);
    */


    qint64 nextTime = getNextTime();
    if (d->speed >= 0 && nextTime != AV_NOPTS_VALUE && nextTime > expectTime + MAX_FRAME_DURATION*1000)
    {
        QnArchiveSyncPlayWrapper* nonConstThis = const_cast<QnArchiveSyncPlayWrapper*>(this);
		
		/*
        qDebug() << "reinitTimeTo=" << QDateTime::fromMSecsSinceEpoch(nextTime/1000).toString("hh:mm:ss.zzz") << 
               "expected time=" << QDateTime::fromMSecsSinceEpoch(expectTime/1000).toString("hh:mm:ss.zzz");
		*/

        nonConstThis->reinitTime(nextTime);
        expectTime = expectedTime();
    }
    else if (d->speed < 0 && nextTime != AV_NOPTS_VALUE && nextTime < expectTime - MAX_FRAME_DURATION*1000)
    {
        /*
        qDebug() << "nextTime=" << QDateTime::fromMSecsSinceEpoch(nextTime/1000).toString("hh:mm:ss.zzz") << 
            "expectTime=" << QDateTime::fromMSecsSinceEpoch(expectTime/1000).toString("hh:mm:ss.zzz")
            << "currentTime=" << QDateTime::fromMSecsSinceEpoch(getDisplayedTimeInternal()/1000).toString("hh:mm:ss.zzz");
        */
        QnArchiveSyncPlayWrapper* nonConstThis = const_cast<QnArchiveSyncPlayWrapper*>(this);
        nonConstThis->reinitTime(nextTime);
        expectTime = expectedTime();
    }

    qint64 displayedTime =  getDisplayedTimeInternal();
    if (displayedTime == AV_NOPTS_VALUE || displayedTime == DATETIME_NOW)
        return displayedTime;
    if (d->speed >= 0) 
        return qMin(expectTime, displayedTime + SYNC_EPS);
    else 
        return qMax(expectTime, displayedTime - SYNC_EPS);
}

void QnArchiveSyncPlayWrapper::onConsumerBlocksReader(QnAbstractStreamDataProvider* reader, bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);

    for (int i = 0; i < d->readers.size(); ++i)
    {
        if (d->readers[i].reader == reader) 
        {
            // Use seek for live too. to clear curent buffer
            if (!d->readers[i].enabled && !value /*&& d->lastJumpTime != DATETIME_NOW*/)
            {
                d->readers[i].reader->setNavDelegate(0);
                if (d->enabled) {
                    qint64 currentTime = getCurrentTime();
                    if (currentTime != AV_NOPTS_VALUE) {
                        d->readers[i].reader->jumpToPreviousFrame(currentTime);
                        d->readers[i].reader->setSpeed(d->speed, currentTime);
                    }
                    else {
                        d->readers[i].reader->setSpeed(d->speed);
                    }
                }
                if (d->readers[i].paused)
                    d->readers[i].reader->resumeMedia();
                if (d->enabled)
                    d->readers[i].reader->setNavDelegate(this);
                d->readers[i].paused = false;
            }
            else if (d->readers[i].enabled && value)
            {
                if (!d->readers[i].reader->isSingleShotMode())
                {
                    d->readers[i].reader->setNavDelegate(0);
                    d->readers[i].reader->pauseMedia();
                    if (d->enabled)
                        d->readers[i].reader->setNavDelegate(this);
                    d->readers[i].paused = true;
                }
            }
            d->readers[i].enabled = !value;
            break;
        }
    }
}

void QnArchiveSyncPlayWrapper::setEnabled(bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    d->enabled = value;
    foreach(const ReaderInfo& info, d->readers)
    {
        info.reader->setNavDelegate(value ? this : 0);
    }
}

bool QnArchiveSyncPlayWrapper::isEnabled() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    return d->enabled;
}
