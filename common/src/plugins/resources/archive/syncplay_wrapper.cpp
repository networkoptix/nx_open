
#include <QtCore/QElapsedTimer>
#include <QtCore/QWaitCondition>

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
    ReaderInfo(QnAbstractArchiveReader* _reader, QnAbstractArchiveDelegate* _oldDelegate, QnlTimeSource* _cam, bool _enabled):
        reader(_reader),
        oldDelegate(_oldDelegate),
        cam(_cam),
        enabled(_enabled),
        buffering(false),
        isEOF(false),
        paused(false),
        timeOffsetUsec(0),
        minTimeUsec(0),
        maxTimeUsec(0)
    {
    }
    QnAbstractArchiveReader* reader;
    QnAbstractArchiveDelegate* oldDelegate;
    QnlTimeSource* cam;
    bool enabled;
    bool buffering;
    bool isEOF;
    bool paused;
    qint64 timeOffsetUsec;
    qint64 minTimeUsec;
    qint64 maxTimeUsec;
};


class QnArchiveSyncPlayWrapperPrivate
{
public:
    void initValues()
    {
        blockSetSpeedSignal = false;
        lastJumpTime = DATETIME_NOW;
        bufferingCnt = 0;
        speed = 1.0;
        enabled = true;
        bufferingTime = AV_NOPTS_VALUE;
        paused = false;
        cachedTime = 0;
        gotCacheTime = 0;
    }


    QnArchiveSyncPlayWrapperPrivate():
      timeMutex(QMutex::Recursive)
    {
        initValues();
        liveModeEnabled = true;
    }

    QList<ReaderInfo> readers;
    mutable QMutex timeMutex;

    bool blockSetSpeedSignal;
    qint64 lastJumpTime;
    int bufferingCnt;
    QElapsedTimer timer;
    double speed;
    
    bool enabled;
    qint64 bufferingTime;
    bool paused;
    bool liveModeEnabled;
    mutable qint64 cachedTime;
    mutable qint64 gotCacheTime;
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
        info.reader->setNavDelegate(0);

        if (info.reader->isMediaPaused())
        {
            info.reader->resumeMedia();
            resumed = true;
        }

        info.reader->setNavDelegate(this);
    }
    bool isBuffering = d->enabled && d->bufferingCnt > 0;
    if (resumed && !isBuffering)
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
    {
        info.reader->setNavDelegate(0);
        rez &= info.reader->isMediaPaused();
        info.reader->setNavDelegate(const_cast<QnArchiveSyncPlayWrapper *>(this));
    }
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

void QnArchiveSyncPlayWrapper::setSkipFramesToTime(qint64 skipTime)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    setJumpTime(skipTime);
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.enabled)
        {
            info.reader->setNavDelegate(0);
            info.reader->setSkipFramesToTime(skipTime);
            info.reader->setNavDelegate(this);
        }
    }
}

bool QnArchiveSyncPlayWrapper::jumpTo(qint64 mksec,  qint64 skipTime)
{
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
            if (mintTime == qint64(AV_NOPTS_VALUE))
                mintTime = curTime;
            else if (curTime != qint64(AV_NOPTS_VALUE))
                mintTime = qMin(mintTime, curTime);
        }
    }
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.enabled) 
        {
            if (mintTime == qint64(AV_NOPTS_VALUE) || info.cam->getCurrentTime() <= mintTime+SYNC_FOR_FRAME_EPS)
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
        if (info.enabled) {
            info.reader->setNavDelegate(0);
            info.reader->previousFrame(mksec);
            info.reader->setNavDelegate(this);
        }
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

    d->readers << ReaderInfo(reader, reader->getArchiveDelegate(), cam, d->enabled);
    
    reader->setArchiveDelegate(new QnSyncPlayArchiveDelegate(reader, this, reader->getArchiveDelegate()));
    reader->setCycleMode(false);

    connect(reader, SIGNAL(beforeJump(qint64)), this, SLOT(onBeforeJump(qint64)), Qt::DirectConnection);
    connect(reader, SIGNAL(jumpOccured(qint64)), this, SLOT(onJumpOccured(qint64)), Qt::DirectConnection);
    connect(reader, SIGNAL(jumpCanceled(qint64)), this, SLOT(onJumpCanceled(qint64)), Qt::DirectConnection);

    if (d->enabled && currentTime != DATETIME_NOW && currentTime != qint64(AV_NOPTS_VALUE)) {
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

    d->speed = value;

    if ((d->lastJumpTime == DATETIME_NOW || displayedTime == DATETIME_NOW) && value < 0)
    {
        // REW from live
        reinitTime(qnSyncTime->currentUSecsSinceEpoch());
    }
    else {
        qint64 et = expectedTime();
        int sign = d->speed >= 0 ? 1 : -1;
        if (d->speed != 0 && value != 0 && sign*(et - displayedTime) > SYNC_EPS) 
            reinitTime(displayedTime);
    }
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
            if (displayTime == qint64(AV_NOPTS_VALUE))
                displayTime = time;
            else if (time != qint64(AV_NOPTS_VALUE))
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
    else if (d->enabled && d->bufferingCnt > 0)
        return d->bufferingTime;

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
            if (displayTime == qint64(AV_NOPTS_VALUE))
                displayTime = time;
            else if (time != qint64(AV_NOPTS_VALUE))
                displayTime = d->speed >= 0 ? qMin(time, displayTime) : qMax(time, displayTime);
        }
    }
    return displayTime;
}

qint64 QnArchiveSyncPlayWrapper::maxArchiveTime() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);

    qint64 result = AV_NOPTS_VALUE;
    foreach(const ReaderInfo& info, d->readers)
    {
        if (info.enabled)
           result = qMax(result, info.reader->endTime());
    }
    return result;
}

void QnArchiveSyncPlayWrapper::reinitTime(qint64 newTime)
{
    Q_D(QnArchiveSyncPlayWrapper);

    if (newTime != qint64(AV_NOPTS_VALUE)) {
        if (newTime == DATETIME_NOW && d->speed < 0)
            d->lastJumpTime = maxArchiveTime();
        else
            d->lastJumpTime = newTime;
    }
    else {
        //d->lastJumpTime = getCurrentTime();
        if (d->lastJumpTime != DATETIME_NOW)
            d->lastJumpTime = getDisplayedTimeInternal();
    }
        
    //qDebug() << "reinitTime=" << QDateTime::fromMSecsSinceEpoch(d->lastJumpTime/1000).toString("hh:mm:ss.zzz");

    d->timer.restart();
}

void QnArchiveSyncPlayWrapper::onBeforeJump(qint64 /*mksec*/)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
}

void QnArchiveSyncPlayWrapper::onJumpCanceled(qint64 /*time*/)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
}

void QnArchiveSyncPlayWrapper::onJumpOccured(qint64 mksec)
{
    Q_UNUSED(mksec)

    Q_D(QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
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
        if(startTime != qint64(AV_NOPTS_VALUE)) {
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
        if(endTime != qint64(AV_NOPTS_VALUE)) {
            result = qMax(result, endTime);
            found = true;
        }
    }
    return found ? result : AV_NOPTS_VALUE;
}

void QnArchiveSyncPlayWrapper::removeArchiveReader(QnAbstractArchiveReader* reader)
{
    erase(reader->getArchiveDelegate());
    onEofReached(0, true);
}

void QnArchiveSyncPlayWrapper::erase(QnAbstractArchiveDelegate* value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->reader->getArchiveDelegate() == value)
        {
            if (i->buffering)
                d->bufferingCnt--;
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
            if (!i->buffering)
                d->bufferingCnt++;
            i->buffering = true;
            if (bufferingTime != (qint64)AV_NOPTS_VALUE)
                d->bufferingTime = bufferingTime;
            break;
            break;
        }
    }
}

bool QnArchiveSyncPlayWrapper::isBuffering() const
{
    Q_D(const QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    for (QList<ReaderInfo>::const_iterator i = d->readers.begin(); i != d->readers.end(); ++i) {
        if (i->enabled && i->buffering)
            return true;
    }
    return false;
}

void QnArchiveSyncPlayWrapper::onBufferingFinished(QnlTimeSource* source)
{
    Q_D(QnArchiveSyncPlayWrapper);

    QMutexLocker lock(&d->timeMutex);
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->cam == source)
        {
            if (i->buffering)
                d->bufferingCnt--;
            i->buffering = false;
            break;
        }
    }


    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->buffering && i->enabled)
            return;
    }

    /*
    // no more buffering
    qint64 bt = d->bufferingTime;
    d->bufferingTime = AV_NOPTS_VALUE;
    qint64 displayTime = getDisplayedTime();
    if (bt != AV_NOPTS_VALUE) {
        if (d->speed >= 0)
            reinitTime(qMax(bt, displayTime));
        else
            reinitTime(qMin(bt, displayTime));
        qDebug() << "correctTime after end of buffering=" << (displayTime - bt)/1000.0;
    }
    else {
        reinitTime(displayTime);
    }
    */
    // reinit time to real position. If reinit time to requested position (it differs if rought jump), redAss can switch item to LQ
    d->bufferingTime = AV_NOPTS_VALUE;
    reinitTime(getDisplayedTime());
}

void QnArchiveSyncPlayWrapper::onEofReached(QnlTimeSource* source, bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    QnAbstractArchiveReader* reader = 0;
    for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
    {
        if (i->cam == source)
        {
            i->isEOF = value;
            reader = i->reader;
            break;
        }
    }
    if (value && d->liveModeEnabled)
    {
        bool allReady = d->speed > 0;
        for (QList<ReaderInfo>::iterator i = d->readers.begin(); i < d->readers.end(); ++i)
        {
            if (i->enabled)
                allReady &= i->isEOF; //(i->isEOF || i->reader->isRealTimeSource());
        }

        if (d->enabled) {
            if (allReady) {
                bool callSync = QThread::currentThread() == qApp->thread();
                if (callSync)
                    jumpTo(DATETIME_NOW, 0);
                else
                    QMetaObject::invokeMethod(this, "jumpToLive", Qt::QueuedConnection); // all items at EOF position. This call may occured from non GUI thread!
            }
        }
        else {
            if (reader)
                reader->jumpTo(DATETIME_NOW, 0); // if sync disabled and items go to archive EOF, jump to live immediatly (without waiting other items)
        }
    }
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
    if (d->lastJumpTime == DATETIME_NOW) {
        d->gotCacheTime = 0;
        return DATETIME_NOW;
    }

    if (d->bufferingCnt > 0) {
        d->gotCacheTime = 0;
        return d->lastJumpTime;
    }

    if (d->bufferingTime != qint64(AV_NOPTS_VALUE)) {
        d->gotCacheTime = 0;
        return d->bufferingTime; // same as last jump time
    }

    foreach(const ReaderInfo& info, d->readers) {
        if (info.enabled && info.buffering) {
            d->gotCacheTime = 0;
            return d->lastJumpTime;
        }
    }
    

    qint64 usecTimer = getUsecTimer();
    if (usecTimer - d->gotCacheTime < 1000ll)
        return d->cachedTime;
    d->gotCacheTime = usecTimer;

    d->cachedTime = getCurrentTimeInternal();
    return d->cachedTime;
}


double sign(double value) { return value >= 0 ? 1 : -1; }

qint64 QnArchiveSyncPlayWrapper::getCurrentTimeInternal() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    /*
    QString s;
    QTextStream str(&s);
    str << "expectTime=" << QDateTime::fromMSecsSinceEpoch(expectTime/1000).toString("hh:mm:ss.zzz");
    str.flush();
    cl_log.log(s, cl_logALWAYS);
    */


    qint64 expectTime = expectedTime();
    qint64 nextTime = getNextTime();
    if (nextTime != qint64(AV_NOPTS_VALUE) && qAbs(nextTime - expectTime) > MAX_FRAME_DURATION*1000)
    {
        QnArchiveSyncPlayWrapper* nonConstThis = const_cast<QnArchiveSyncPlayWrapper*>(this);
        if ((nextTime > expectTime && d->speed >= 0) || (nextTime < expectTime && d->speed < 0))
            nonConstThis->reinitTime(nextTime); // data hole
        else
            nonConstThis->reinitTime(nextTime + MAX_FRAME_DURATION/2*1000ll * sign(d->speed)); // stream is playing slower than need. do not release expected time too far away
        expectTime = expectedTime();
    }

    qint64 displayedTime =  getDisplayedTimeInternal();
    if (displayedTime == qint64(AV_NOPTS_VALUE) || displayedTime == DATETIME_NOW)
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
                    if (currentTime != qint64(AV_NOPTS_VALUE)) {
                        setJumpTime(currentTime);
                        d->readers[i].reader->jumpToPreviousFrame(currentTime);
                        d->readers[i].reader->setSpeed(d->speed, currentTime);
                    }
                    else {
                        d->readers[i].reader->setSpeed(d->speed);
                    }
                }
                if (d->readers[i].paused)
                    d->readers[i].reader->resume();
                if (d->enabled)
                    d->readers[i].reader->setNavDelegate(this);
                d->readers[i].paused = false;
            }
            else if (d->readers[i].enabled && value)
            {
                if (!d->readers[i].reader->isSingleShotMode())
                {
                    d->readers[i].reader->setNavDelegate(0);
                    // use pause instead of pauseMedia. Prevent isMediaPaused=true value. So, pause thread physically but not change any playback logic 
                    d->readers[i].reader->pause(); 
                    if (d->enabled)
                        d->readers[i].reader->setNavDelegate(this);
                    d->readers[i].paused = true;
                    if (d->readers[i].buffering)
                        onBufferingFinished(d->readers[i].cam);
                }
            }
            d->readers[i].enabled = !value;
            break;
        }
    }
}

void QnArchiveSyncPlayWrapper::disableSync()
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    if (!d->enabled)
        return;
    d->enabled = false;
    foreach(const ReaderInfo& info, d->readers) {
        info.reader->setNavDelegate(0);
        // check if reader stay in EOF position, but position is not LIVE because waiting other items. At this case seek to live
        if (info.isEOF)
            info.reader->jumpTo(DATETIME_NOW, 0);
    }
}

void QnArchiveSyncPlayWrapper::enableSync(qint64 currentTime, float currentSpeed)
{
    Q_D(QnArchiveSyncPlayWrapper);
    QMutexLocker lock(&d->timeMutex);
    if (d->enabled)
        return;
    d->enabled = true;
    d->speed = currentSpeed;
    bool isPaused = (currentSpeed == 0);
    foreach(const ReaderInfo& info, d->readers) 
    {
        if (info.enabled)
        {
            bool isItemPaused = info.reader->isMediaPaused();
            if (!isItemPaused && isPaused)
                info.reader->pauseMedia();

            if (currentTime != qint64(AV_NOPTS_VALUE)) {
                setJumpTime(currentTime);
                info.reader->jumpToPreviousFrame(currentTime);
                info.reader->setSpeed(currentSpeed, currentTime);
            }
            else {
                info.reader->setSpeed(currentSpeed);
            }
            if (isItemPaused && !isPaused)
                info.reader->resumeMedia();

            info.reader->setNavDelegate(this);
        }
    }
}

bool QnArchiveSyncPlayWrapper::isEnabled() const
{
    Q_D(const QnArchiveSyncPlayWrapper);
    return d->enabled;
}

void QnArchiveSyncPlayWrapper::setLiveModeEnabled(bool value)
{
    Q_D(QnArchiveSyncPlayWrapper);
    d->liveModeEnabled = value;    
}
    
void QnArchiveSyncPlayWrapper::jumpToLive()
{
    jumpTo(DATETIME_NOW, 0);
}
