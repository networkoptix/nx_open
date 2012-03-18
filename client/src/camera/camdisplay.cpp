#include "camdisplay.h"

#include "core/datapacket/mediadatapacket.h"
#include "videostreamdisplay.h"
#include "audiostreamdisplay.h"
#include "decoders/audio/ffmpeg_audio.h"
#include "utils/common/synctime.h"

#include <QDateTime>

#if defined(Q_OS_MAC)
#include <CoreServices/CoreServices.h>
#elif defined(Q_OS_WIN)
#include <qt_windows.h>
#endif
#include "utils/common/util.h"
#include "plugins/resources/archive/archive_stream_reader.h"
#include "core/resource/camera_resource.h"

Q_GLOBAL_STATIC(QMutex, activityMutex)
static qint64 activityTime = 0;
static const int TRY_HIGH_QUALITY_INTERVAL = 1000 * 30;
static const int QUALITY_SWITCH_INTERVAL = 1000 * 5; // delay between high quality switching attempts
static const int HIGH_QUALITY_RETRY_COUNTER = 1;


QSet<CLCamDisplay*> CLCamDisplay::m_allCamDisplay;
QMutex CLCamDisplay::m_qualityMutex;
qint64 CLCamDisplay::m_lastQualitySwitchTime;

static void updateActivity()
{
    QMutexLocker locker(activityMutex());

    if (qnSyncTime->currentMSecsSinceEpoch() >= activityTime)
    {
#ifdef Q_OS_MAC
        UpdateSystemActivity(UsrActivity);
#elif defined(Q_OS_WIN)
        // disable screen saver ### should we enable it back on exit?
        static bool screenSaverDisabled = SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, 0, SPIF_SENDWININICHANGE);
        Q_UNUSED(screenSaverDisabled)

        // don't sleep
        if (QSysInfo::windowsVersion() < QSysInfo::WV_VISTA)
            SetThreadExecutionState(ES_USER_PRESENT | ES_CONTINUOUS);
        else
            SetThreadExecutionState(ES_AWAYMODE_REQUIRED | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
#endif
        // Update system activity timer once per 20 seconds
        activityTime = qnSyncTime->currentMSecsSinceEpoch() + 20000;
    }
}


// a lot of small audio packets in bluray HD audio codecs. So, previous size 7 is not enought
#define CL_MAX_DISPLAY_QUEUE_SIZE 15
#define CL_MAX_DISPLAY_QUEUE_FOR_SLOW_SOURCE_SIZE 15
#define AUDIO_BUFF_SIZE (4000) // ms

static const qint64 MIN_VIDEO_DETECT_JUMP_INTERVAL = 100 * 1000; // 100ms
static const qint64 MIN_AUDIO_DETECT_JUMP_INTERVAL = MIN_VIDEO_DETECT_JUMP_INTERVAL + AUDIO_BUFF_SIZE*1000;
//static const int MAX_VALID_SLEEP_TIME = 1000*1000*5;
static const int MAX_VALID_SLEEP_LIVE_TIME = 1000 * 500; // 5 seconds as most long sleep time
static const int SLOW_COUNTER_THRESHOLD = 24;
static const double FPS_EPS = 0.0001;

static const int DEFAULT_DELAY_OVERDRAFT = 100 * 1000;

CLCamDisplay::CLCamDisplay(bool generateEndOfStreamSignal)
    : QnAbstractDataConsumer(CL_MAX_DISPLAY_QUEUE_SIZE),
      m_delay(DEFAULT_DELAY_OVERDRAFT), // do not put a big value here to avoid cpu usage in case of zoom out
      m_playAudio(false),
      m_speed(1.0),
      m_prevSpeed(1.0),
      m_needChangePriority(false),
      m_hadAudio(false),
      m_lastAudioPacketTime(0),
      m_syncAudioTime(AV_NOPTS_VALUE),
      m_totalFrames(0),
      m_iFrames(0),
      m_lastVideoPacketTime(0),
      m_lastDecodedTime(AV_NOPTS_VALUE),
      m_previousVideoTime(0),
      m_lastNonZerroDuration(0),
      m_lastSleepInterval(0),
      //m_previousVideoDisplayedTime(0),
      m_afterJump(false),
      m_bofReceived(false),
      m_displayLasts(0),
      m_ignoringVideo(false),
      mGenerateEndOfStreamSignal(generateEndOfStreamSignal),
      m_isRealTimeSource(true),
      m_videoBufferOverflow(false),
      m_singleShotMode(false),
      m_singleShotQuantProcessed(false),
      m_jumpTime(0),
      m_playingCompress(0),
      m_playingBitrate(0),
      m_tooSlowCounter(0),
      m_lightCpuMode(QnAbstractVideoDecoder::DecodeMode_Full),
      m_lastFrameDisplayed(CLVideoStreamDisplay::Status_Displayed),
      m_realTimeHurryUp(0),
      m_extTimeSrc(0),
      m_useMtDecoding(false),
      m_buffering(0),
      m_executingJump(0),
      skipPrevJumpSignal(0),
      m_processedPackets(0),
      m_toLowQSpeed(1000.0),
      m_delayedFrameCnt(0),
      m_emptyPacketCounter(0),
      m_hiQualityRetryCounter(0),
      m_isStillImage(false),
      m_isLongWaiting(false),
      m_executingChangeSpeed(false),
      m_eofSignalSended(false)
{
    m_storedMaxQueueSize = m_dataQueue.maxSize();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_display[i] = 0;
        m_nextReverseTime[i] = AV_NOPTS_VALUE;
    }
    m_audioDisplay = new CLAudioStreamDisplay(AUDIO_BUFF_SIZE);

    QMutexLocker lock(&m_qualityMutex);
    m_allCamDisplay << this;
}

CLCamDisplay::~CLCamDisplay()
{
    {
        QMutexLocker lock(&m_qualityMutex);
        m_allCamDisplay.remove(this);
        foreach(CLCamDisplay* display, m_allCamDisplay)
            display->resetQualityStatistics();
    }

    Q_ASSERT(!m_runing);
    stop();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        delete m_display[i];

    clearVideoQueue();
    delete m_audioDisplay;
}

void CLCamDisplay::pause()
{
    QnAbstractDataConsumer::pause();

    m_audioDisplay->suspend();
}

void CLCamDisplay::resume()
{
    m_delay.afterdelay();
    m_singleShotMode = false;
    m_audioDisplay->resume();
    QnAbstractDataConsumer::resume();
}

void CLCamDisplay::addVideoChannel(int index, QnAbstractRenderer* vw, bool canDownscale)
{
    Q_ASSERT(index < CL_MAX_CHANNELS);

    delete m_display[index];
    m_display[index] = new CLVideoStreamDisplay(canDownscale);
    m_display[index]->setDrawer(vw);
}

QImage CLCamDisplay::getScreenshot(int channel)
{
    return m_display[channel]->getScreenshot();
}

void CLCamDisplay::hurryUpCheck(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime)
{
    bool isVideoCamera = qSharedPointerDynamicCast<QnVirtualCameraResource>(vd->dataProvider->getResource()) != 0;
    if (isVideoCamera)
        hurryUpCheckForCamera(vd, speed, needToSleep, realSleepTime);
    else
        hurryUpCheckForLocalFile(vd, speed, needToSleep, realSleepTime);
}


bool CLCamDisplay::canSwitchQuality()
{
    QMutexLocker lock(&m_qualityMutex);
    qint64 currentTime = qnSyncTime->currentMSecsSinceEpoch();
    if (currentTime - m_lastQualitySwitchTime < QUALITY_SWITCH_INTERVAL)
        return false;
    m_lastQualitySwitchTime = currentTime;
    return true;
}
void CLCamDisplay::hurryUpCheckForCamera(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime)
{
    if (vd->flags & QnAbstractMediaData::MediaFlags_LIVE)
        return;
    if (vd->flags & QnAbstractMediaData::MediaFlags_Ignore)
        return;

    QnArchiveStreamReader* reader = dynamic_cast<QnArchiveStreamReader*> (vd->dataProvider);
    if (reader)
    {
        if (realSleepTime <= -1000*1000) 
        {
            m_delayedFrameCnt = qMax(0, m_delayedFrameCnt);
            m_delayedFrameCnt++;
            if (m_delayedFrameCnt > 10 && reader->getQuality() != MEDIA_Quality_Low /*&& canSwitchQuality()*/)
            {
                bool fastSwitch = false; // m_dataQueue.size() >= m_dataQueue.maxSize()*0.75;
                // if CPU is slow use fat switch, if problem with network - use slow switch to save already received data
                reader->setQuality(MEDIA_Quality_Low, fastSwitch);
                m_toLowQSpeed = speed;
                //m_toLowQTimer.restart();
            }
        }
        else if (realSleepTime >= 0)
        {
            m_delayedFrameCnt = qMin(0, m_delayedFrameCnt);
            m_delayedFrameCnt--;
            if (m_delayedFrameCnt < -10 && m_dataQueue.size() >= m_dataQueue.size()*0.75)
            {
                if (qAbs(speed) < m_toLowQSpeed || m_toLowQSpeed < 0 && speed > 0)
                {
                    reader->setQuality(MEDIA_Quality_High, true); // speed decreased, try to Hi quality again
                }
                //else if(qAbs(speed) < 1.0 + FPS_EPS && m_toLowQTimer.elapsed() >= TRY_HIGH_QUALITY_INTERVAL)
                else if(qAbs(speed) < 1.0 + FPS_EPS && m_hiQualityRetryCounter < HIGH_QUALITY_RETRY_COUNTER)
                {
                    if (canSwitchQuality()) {
                        reader->setQuality(MEDIA_Quality_High, false); // speed decreased, try to Hi quality now
                        m_hiQualityRetryCounter++;
                    }
                }
            }
        }
    }
}

void CLCamDisplay::resetQualityStatistics()
{
    m_hiQualityRetryCounter = 0;
}

void CLCamDisplay::hurryUpCheckForLocalFile(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime)
{
    if (qAbs(speed) > 1.0 + FPS_EPS)
    {
        if (realSleepTime < 0)
        {
            if (realSleepTime > -200*1000 && m_lightCpuMode == QnAbstractVideoDecoder::DecodeMode_Full)
            {
                setLightCPUMode(QnAbstractVideoDecoder::DecodeMode_Fast);
            }

            else if (m_iFrames > 1) {
                qint64 avgGopDuration = ((qint64)needToSleep * m_totalFrames)/m_iFrames;
                if (realSleepTime < qMin(-400*1000ll, -avgGopDuration))
                {
                    ; //setLightCPUMode(QnAbstractVideoDecoder::DecodeMode_Fastest);
                }
                else if (vd->flags & AV_PKT_FLAG_KEY)
                    setLightCPUMode(QnAbstractVideoDecoder::DecodeMode_Fast);
            }
        }
        else if (vd->flags & AV_PKT_FLAG_KEY) {
            if (m_lightCpuMode == QnAbstractVideoDecoder::DecodeMode_Fastest)
                setLightCPUMode(QnAbstractVideoDecoder::DecodeMode_Fast);
        }
    }
}

int CLCamDisplay::getBufferingMask()
{
    int channelMask = 0;
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) 
        channelMask = channelMask*2  + 1;
    return channelMask;
};

float sign(float value)
{
    if (value == 0)
        return 0;
    return value > 0 ? 1 : -1;
}

void CLCamDisplay::display(QnCompressedVideoDataPtr vd, bool sleep, float speed)
{
//    cl_log.log("queueSize=", m_dataQueue.size(), cl_logALWAYS);
//    cl_log.log(QDateTime::fromMSecsSinceEpoch(vd->timestamp/1000).toString("hh.mm.ss.zzz"), cl_logALWAYS);

    // simple data provider/streamer/streamreader has the same delay, but who cares ?
    // to avoid cpu usage in case of a lot data in queue(zoomed out on the scene, lets add same delays here )
    quint64 currentTime = vd->timestamp;

    m_totalFrames++;
    if (vd->flags & AV_PKT_FLAG_KEY)
        m_iFrames++;


    // in ideal world data comes to queue at the same speed as it goes out
    // but timer on the sender side runs at a bit different rate in comparison to the timer here
    // adaptive delay will not solve all problems => need to minus little appendix based on queue size
    qint32 needToSleep;

    /*
    if (vd->flags & QnAbstractMediaData::MediaFlags_LIVE)
    {
        needToSleep = vd->timestamp - qnSyncTime->currentMSecsSinceEpoch()*1000;
    }
    else 
    */
    if (vd->flags & QnAbstractMediaData::MediaFlags_BOF)
        m_lastSleepInterval = needToSleep = 0;
    
    if (vd->flags & AV_REVERSE_BLOCK_START)
        needToSleep = m_lastSleepInterval;
    else {
        needToSleep = m_lastSleepInterval = (currentTime - m_previousVideoTime) * 1.0/qAbs(speed);
    }

    //qDebug() << "needToSleep" << needToSleep/1000.0;

    if (m_isRealTimeSource)
    {
        if (m_dataQueue.size() > 0) {
            sleep = false;
            m_realTimeHurryUp = 5;
        }
        else if (m_realTimeHurryUp)
        {
            m_realTimeHurryUp--;
            if (m_realTimeHurryUp)
                sleep = false;
            else {
                m_delay.afterdelay();
                m_delay.addQuant(-needToSleep);
                m_realTimeHurryUp = false;
            }
        }
    }

    m_previousVideoTime = currentTime;

    //===== to avoid unrelated streams / stop play delays
    if (needToSleep < 0)
        needToSleep = 0;

    if (needToSleep > MAX_VALID_SLEEP_TIME) // in case of key frame only and sliding archive slider forward - would not look good; need to do smth
        needToSleep = 0;
    //=========

    if (sleep && m_lastFrameDisplayed != CLVideoStreamDisplay::Status_Buffered)
    {
        qint64 realSleepTime = 0;
        if (useSync(vd)) 
        {
            qint64 displayedTime = getCurrentTime();
            //bool isSingleShot = vd->flags & QnAbstractMediaData::MediaFlags_SingleShot;
            if (speed != 0  && displayedTime != AV_NOPTS_VALUE && m_lastFrameDisplayed == CLVideoStreamDisplay::Status_Displayed &&
                !(vd->flags & QnAbstractMediaData::MediaFlags_BOF))
            {
                Q_ASSERT(!(vd->flags & QnAbstractMediaData::MediaFlags_Ignore));
                //QTime t;
                //t.start();
                int speedSign = speed >= 0 ? 1 : -1;
                bool firstWait = true;
                QTime sleepTimer;
                sleepTimer.start();
                while (!m_afterJump && !m_buffering && !m_needStop && sign(m_speed) == sign(speed) && useSync(vd)) 
                {
                    qint64 ct = m_extTimeSrc->getCurrentTime();
                    if (ct != DATETIME_NOW && speedSign *(displayedTime - ct) > 0)
                    {
					    if (firstWait)
                        {
                            m_isLongWaiting = speedSign*(displayedTime - ct) > MAX_FRAME_DURATION*1000 && speedSign*(displayedTime - m_jumpTime)  > MAX_FRAME_DURATION*1000;
                            
							/*
                            qDebug() << "displayedTime=" << QDateTime::fromMSecsSinceEpoch(displayedTime/1000).toString("hh:mm:ss.zzz")
                                     << " currentTime=" << QDateTime::fromMSecsSinceEpoch(ct/1000).toString("hh:mm:ss.zzz")
                                     << " wait=" << (displayedTime - ct)/1000.0;
							*/
                            
                            firstWait = false;
                        }
                        QnSleep::msleep(1);
                    }
                    else {
                        break;
                    }
                }
                m_isLongWaiting = false;
                /*
                if (sleepTimer.elapsed() > 0)
                    realSleepTime = sleepTimer.elapsed()*1000;
                else 
                    realSleepTime = sign * (displayedTime - m_extTimeSrc->expectedTime());
                */
                realSleepTime = speedSign * (displayedTime - m_extTimeSrc->expectedTime());
                //qDebug() << "sleepTimer=" << sleepTimer.elapsed() << "extSleep=" << realSleepTime;
            }
        }
        else {
            realSleepTime = m_lastFrameDisplayed == CLVideoStreamDisplay::Status_Displayed ? m_delay.sleep(needToSleep, needToSleep*qAbs(speed)*2) : m_delay.addQuant(needToSleep);
        }

        //qDebug() << "sleep time: " << needToSleep/1000.0 << "  real:" << realSleepTime/1000.0;
        hurryUpCheck(vd, speed, needToSleep, realSleepTime);
    }
    int channel = vd->channelNumber;

    CLVideoDecoderOutput::downscale_factor scaleFactor = CLVideoDecoderOutput::factor_any;
    if (channel > 0 && m_display[0]) // if device has more than one channel
    {
        // this here to avoid situation where different channels have different down scale factor; it might lead to diffrent size
        scaleFactor = m_display[0]->getCurrentDownscaleFactor(); // [0] - master channel
    }
    if (vd->flags & QnAbstractMediaData::MediaFlags_StillImage)
        scaleFactor = CLVideoDecoderOutput::factor_1; // do not downscale still images


    if (m_display[channel])
    {
        // sometimes draw + decoding takes a lot of time. so to be able always sync video and audio we MUST not draw
        QTime displayTime;
        displayTime.restart();

        bool ignoreVideo = vd->flags & QnAbstractMediaData::MediaFlags_Ignore;
        bool draw = !ignoreVideo && (sleep || (m_displayLasts * 1000 < needToSleep)); // do not draw if computer is very slow and we still wanna sync with audio

        // If there are multiple channels for this timestamp use only one of them
        //if (channel == 0 && draw)
        //    m_previousVideoDisplayedTime = currentTime;

        CL_LOG(cl_logDEBUG2)
        {
            if (vd->flags & QnAbstractMediaData::MediaFlags_Ignore)
                cl_log.log(QLatin1String("Ignoring frame "), (int)vd->timestamp, cl_logDEBUG2);
            else
                cl_log.log(QLatin1String("Playing frame "), (int)vd->timestamp, cl_logDEBUG2);

            if (!draw)
                cl_log.log(QLatin1String("skip drawing frame!!"), displayTime.elapsed(), cl_logDEBUG2);

        }

        if (draw) {
            /*
            if (m_lastDisplayTimer.elapsed() < 1000)
                draw = false;
            else
                m_lastDisplayTimer.restart();
            */
            updateActivity();
        }
        if (!(vd->flags & QnAbstractMediaData::MediaFlags_Ignore)) 
        {
            QMutexLocker lock(&m_timeMutex);
            m_lastDecodedTime = vd->timestamp;
        }

        m_lastFrameDisplayed = m_display[channel]->dispay(vd, draw, scaleFactor);

        if (m_lastFrameDisplayed == CLVideoStreamDisplay::Status_Displayed)
        {
            if (speed < 0)
                m_nextReverseTime[channel] = m_display[channel]->nextReverseTime();

            m_timeMutex.lock();
            if (m_buffering && m_executingJump == 0) 
            {
                m_buffering &= ~(1 << vd->channelNumber);
                m_timeMutex.unlock();
                if (m_buffering == 0 && m_extTimeSrc)
                    m_extTimeSrc->onBufferingFinished(this);
            }
            else
                m_timeMutex.unlock();
        }

        if (!ignoreVideo && m_buffering)
        {
            // Frame does not displayed for some reason (and it is not ignored frames)
            QnArchiveStreamReader* archive = dynamic_cast<QnArchiveStreamReader*>(vd->dataProvider);
            if (archive && archive->isSingleShotMode())
                archive->needMoreData();
        }

        if (!sleep)
            m_displayLasts = displayTime.elapsed(); // this is how long would i take to draw frame.

        //m_display[channel]->dispay(vd, sleep, scale_factor);
        //cl_log.log(" video queue size = ", m_videoQueue[0].size(),  cl_logALWAYS);
    }

}

void CLCamDisplay::onBeforeJump(qint64 time)
{
    onRealTimeStreamHint(time == DATETIME_NOW && m_speed >= 0);

    /*
    if (time < 1000000ll * 100000)
        cl_log.log("before jump to ", time, cl_logWARNING);
    else
        cl_log.log("before jump to ", QDateTime::fromMSecsSinceEpoch(time/1000).toString(), cl_logWARNING);
    */
    
    QMutexLocker lock(&m_timeMutex);

    m_lastDecodedTime = AV_NOPTS_VALUE;
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
        m_nextReverseTime[i] = AV_NOPTS_VALUE;
        m_display[i]->blockTimeValue(time);
    }

    m_emptyPacketCounter = 0;
    clearUnprocessedData();

    if (skipPrevJumpSignal > 0) {
        skipPrevJumpSignal--;
        return;
    }
    setSingleShotMode(false);

    
    m_executingJump++;

    m_buffering = getBufferingMask();

    if (m_executingJump > 1)
        clearUnprocessedData();
    m_processedPackets = 0;
}

void CLCamDisplay::onJumpOccured(qint64 time)
{
    /*
    if (time < 1000000ll * 100000)
        cl_log.log("after jump to ", time, cl_logWARNING);
    else
        cl_log.log("after jump to ", QDateTime::fromMSecsSinceEpoch(time/1000).toString(), cl_logWARNING);
    */       
    if (m_extTimeSrc)
        m_extTimeSrc->onBufferingStarted(this, time);

    QMutexLocker lock(&m_timeMutex);
    m_afterJump = true;
    m_bofReceived = false;
    m_buffering = getBufferingMask();
    m_lastDecodedTime = AV_NOPTS_VALUE;
    //clearUnprocessedData();
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
        m_nextReverseTime[i] = AV_NOPTS_VALUE;
        m_display[i]->blockTimeValue(time);
    }
    m_singleShotMode = false;
    m_jumpTime = time;

    m_executingJump--;
    m_processedPackets = 0;
    m_delayedFrameCnt = 0;
}

void CLCamDisplay::onJumpCanceled(qint64 /*time*/)
{
    skipPrevJumpSignal++;
}

void CLCamDisplay::afterJump(QnAbstractMediaDataPtr media)
{
    QnCompressedVideoDataPtr vd = qSharedPointerDynamicCast<QnCompressedVideoData>(media);
    //cl_log.log("after jump.time=", QDateTime::fromMSecsSinceEpoch(media->timestamp/1000).toString(), cl_logWARNING);

    clearVideoQueue();
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) 
        m_display[i]->afterJump();

    QMutexLocker lock(&m_timeMutex);
    m_lastAudioPacketTime = media->timestamp;
    m_lastVideoPacketTime = media->timestamp;
    m_previousVideoTime = media->timestamp;
    //m_previousVideoDisplayedTime = 0;
    m_totalFrames = 0;
    m_iFrames = 0;
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
        if (vd && !(vd->flags & QnAbstractMediaData::MediaFlags_Ignore))
            m_display[i]->blockTimeValue(media->timestamp);
        m_display[i]->unblockTimeValue();
    }

    m_audioDisplay->clearAudioBuffer();

    if (mGenerateEndOfStreamSignal)
        emit reachedTheEnd();
}

void CLCamDisplay::onReaderPaused()
{
    setSingleShotMode(true);
}

void CLCamDisplay::onReaderResumed()
{
    setSingleShotMode(false);
}

void CLCamDisplay::onPrevFrameOccured()
{
    setSingleShotMode(false);
}

void CLCamDisplay::onNextFrameOccured()
{
    setSingleShotMode(true);
}

void CLCamDisplay::setSingleShotMode(bool single)
{
    m_singleShotMode = single;
    m_singleShotQuantProcessed = false;
}

void CLCamDisplay::setSpeed(float speed)
{
    QMutexLocker lock(&m_timeMutex);
    if (qAbs(speed-m_speed) > FPS_EPS)
    {
        m_executingChangeSpeed = true;
        if (speed < 0 && m_speed >= 0) {
            for (int i = 0; i < CL_MAX_CHANNELS; ++i)
                m_nextReverseTime[i] = AV_NOPTS_VALUE;
        }
        m_speed = speed;
    }
};

void CLCamDisplay::processNewSpeed(float speed)
{
    if (qAbs(speed - 1.0) > FPS_EPS && qAbs(speed) > FPS_EPS)
    {
        m_audioDisplay->clearAudioBuffer();
    }

    if (qAbs(speed) > 1.0 + FPS_EPS || speed < 0)
    {
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; i++)
            m_display[i]->setMTDecoding(true);
    }
    else {
        setMTDecoding(m_useMtDecoding);
    }

    if (speed < 0 && m_prevSpeed >= 0)
        m_buffering = getBufferingMask(); // decode first gop is required some time

    if (speed >= 0 && m_prevSpeed < 0 || speed < 0 && m_prevSpeed >= 0)
    {
        m_dataQueue.clear();
        clearVideoQueue();
        QMutexLocker lock(&m_timeMutex);
        m_lastDecodedTime = AV_NOPTS_VALUE;
        for (int i = 0; i < CL_MAX_CHANNELS; ++i)
            m_nextReverseTime[i] = AV_NOPTS_VALUE;
    }
    if (qAbs(speed) > 1.0) {
        m_storedMaxQueueSize = m_dataQueue.maxSize();
        m_dataQueue.setMaxSize(CL_MAX_DISPLAY_QUEUE_FOR_SLOW_SOURCE_SIZE);
        m_delay.setMaxOverdraft(-1);
    }
    else {
        m_dataQueue.setMaxSize(m_storedMaxQueueSize);
        m_delay.setMaxOverdraft(DEFAULT_DELAY_OVERDRAFT);
        m_delay.afterdelay();
    }
    m_tooSlowCounter = 0;
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        if (m_display[i])
            m_display[i]->setSpeed(speed);
    }
    setLightCPUMode(QnAbstractVideoDecoder::DecodeMode_Full);
    m_executingChangeSpeed = false;
}

bool CLCamDisplay::useSync(QnCompressedVideoDataPtr vd)
{
    //return m_extTimeSrc && !(vd->flags & (QnAbstractMediaData::MediaFlags_LIVE | QnAbstractMediaData::MediaFlags_BOF)) && !m_singleShotMode;
    return m_extTimeSrc && m_extTimeSrc->isEnabled() && !(vd->flags & QnAbstractMediaData::MediaFlags_LIVE);
}

bool CLCamDisplay::canAcceptData() const
{
    if (m_processedPackets < m_dataQueue.maxSize())
        return m_dataQueue.size() <= m_processedPackets;
    else 
        return QnAbstractDataConsumer::canAcceptData();
        //return m_dataQueue.mediaLength() < MAX_QUEUE_LENGTH;
}

bool CLCamDisplay::processData(QnAbstractDataPacketPtr data)
{

    QnAbstractMediaDataPtr media = qSharedPointerDynamicCast<QnAbstractMediaData>(data);
    if (!media)
        return true;

    QnMetaDataV1Ptr metadata = qSharedPointerDynamicCast<QnMetaDataV1>(data);
    if (metadata) {
        m_lastMetadata[metadata->channelNumber] = metadata;
        return true;
    }

    QnCompressedVideoDataPtr vd = qSharedPointerDynamicCast<QnCompressedVideoData>(data);
    QnCompressedAudioDataPtr ad = qSharedPointerDynamicCast<QnCompressedAudioData>(data);
    //if (!vd && !ad)
    //    return true;


    m_processedPackets++;

    bool mediaIsLive = media->flags & QnAbstractMediaData::MediaFlags_LIVE;
    if (mediaIsLive != m_isRealTimeSource && !m_buffering)
        onRealTimeStreamHint(mediaIsLive);

    float speed = m_speed;
    bool speedIsNegative = speed < 0;
    bool dataIsNegative = media->flags & QnAbstractMediaData::MediaFlags_Reverse;
    if (speedIsNegative != dataIsNegative)
        return true; // skip data

    if (m_prevSpeed != speed) 
    {
        processNewSpeed(speed);
        m_prevSpeed = speed;
    }

    if (vd)
    {
        m_ignoringVideo = vd->flags & QnAbstractMediaData::MediaFlags_Ignore;
        if (m_lastMetadata[vd->channelNumber] && m_lastMetadata[vd->channelNumber]->containTime(vd->timestamp))
            vd->motion = m_lastMetadata[vd->channelNumber];
    }
    else if (ad)
    {
        if (speed < 0) {
            m_lastAudioPacketTime = ad->timestamp;
            return true; // ignore audio packet to prevent after jump detection
        }
    }
    //else
    //    return true;
    
    m_isStillImage = media->flags & QnAbstractMediaData::MediaFlags_StillImage;

    bool isReversePacket = media->flags & QnAbstractMediaData::MediaFlags_Reverse;
    bool isReverseMode = speed < 0.0;
    if (isReverseMode != isReversePacket)
        return true;

    if (m_needChangePriority)
    {
        setPriority(m_playAudio ? QThread::HighestPriority : QThread::NormalPriority);
        m_needChangePriority = false;
    }

    if( m_afterJump)
    {
        //if (!(media->flags & QnAbstractMediaData::MediaFlags_BOF)) // jump finished, but old data received)
        //    return true; // jump finished, but old data received
        if (media->flags & QnAbstractMediaData::MediaFlags_BOF)
            m_bofReceived = true;

        if (!m_bofReceived)
            return true; // jump finished, but old data received

        // Some clips has very low key frame rate. This condition protect audio buffer overflowing and improve seeking for such clips
        if (ad && ad->timestamp < m_jumpTime - AUDIO_BUFF_SIZE/2*1000)
            return true; // skip packet
        // clear everything we can
        m_bofReceived = false;
        afterJump(media);
        QMutexLocker lock(&m_timeMutex);
        if (m_executingJump == 0) 
            m_afterJump = false;
        //cl_log.log("ProcessData 2", QDateTime::fromMSecsSinceEpoch(vd->timestamp/1000).toString("hh:mm:ss.zzz"), cl_logALWAYS);
    }

    QnEmptyMediaDataPtr emptyData = qSharedPointerDynamicCast<QnEmptyMediaData>(data);

    if (emptyData)
    {
        m_emptyPacketCounter++;
        // empty data signal about EOF, or read/network error. So, check counter bofore EOF signaling
        if (m_emptyPacketCounter >= 3)
        {
            bool isLive = emptyData->flags & QnAbstractMediaData::MediaFlags_LIVE;
            if (m_extTimeSrc && !isLive) {
            	m_extTimeSrc->onEofReached(this, true); // jump to live if needed
                m_eofSignalSended = true;
            }

            /*
            // One camera from several sync cameras may reach BOF/EOF
		    // move current time position to the edge to prevent other cameras blocking
            m_nextReverseTime = m_lastDecodedTime = emptyData->timestamp;
            for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
                m_display[i]->setLastDisplayedTime(m_lastDecodedTime);
            }
            */
            m_timeMutex.lock();
            m_lastDecodedTime = AV_NOPTS_VALUE;
            for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
                m_display[i]->setLastDisplayedTime(AV_NOPTS_VALUE);
                m_nextReverseTime[i] = AV_NOPTS_VALUE;
            }

            if (m_buffering && m_executingJump == 0) 
            {
                m_buffering = 0;
                m_timeMutex.unlock();
                if (m_extTimeSrc)
                    m_extTimeSrc->onBufferingFinished(this);
            }
            else 
                m_timeMutex.unlock();
        }
        return true;
    }
    else 
    {
        if (m_extTimeSrc && m_eofSignalSended) {
        	m_extTimeSrc->onEofReached(this, false);
            m_eofSignalSended = false;
        }
        m_emptyPacketCounter = 0;
    }

    bool flushCurrentBuffer = false;
    bool audioParamsChanged = ad && m_playingFormat != ad->format;
    if (((media->flags & QnAbstractMediaData::MediaFlags_AfterEOF) || audioParamsChanged) &&
        m_videoQueue[0].size() > 0)
    {
        // skip data (play current buffer
        flushCurrentBuffer = true;
    }
    else if ((media->flags & QnAbstractMediaData::MediaFlags_AfterEOF) && vd) {
        m_display[vd->channelNumber]->waitForFramesDisplaed();
    }

    if (ad && !flushCurrentBuffer)
    {
        if (audioParamsChanged)
        {
            delete m_audioDisplay;
            m_audioDisplay = new CLAudioStreamDisplay(AUDIO_BUFF_SIZE);
            m_playingFormat = ad->format;
        }

        // after seek, when audio is shifted related video (it is often), first audio packet will be < seek threshold
        // so, second afterJump is generated after several video packet. To prevent it, increase jump detection interval for audio

        if (ad->timestamp && ad->timestamp - m_lastAudioPacketTime < -MIN_AUDIO_DETECT_JUMP_INTERVAL)
        {
            afterJump(ad);
        }

        m_lastAudioPacketTime = ad->timestamp;

        // we synch video to the audio; so just put audio in player with out thinking
        if (m_playAudio && qAbs(speed-1.0) < FPS_EPS)
        {
            if (m_audioDisplay->msInBuffer() > AUDIO_BUFF_SIZE)
            {
                // Audio buffer too large. waiting
                QnSleep::msleep(40);
            }

            m_audioDisplay->putData(ad, nextVideoImageTime(0));
            m_hadAudio = true;
        }
        else if (m_hadAudio)
        {
            m_audioDisplay->enqueueData(ad, nextVideoImageTime(0));
        }
    }

    if (vd || flushCurrentBuffer)
    {
        bool result = !flushCurrentBuffer;
        int channel = vd ? vd->channelNumber : 0;
        if (flushCurrentBuffer)
        {
            if (m_singleShotMode && m_singleShotQuantProcessed)
                return false;
            vd = QnCompressedVideoDataPtr();
        }
        else
        {
            if (vd->timestamp - m_lastVideoPacketTime < -MIN_VIDEO_DETECT_JUMP_INTERVAL)
            {
                if (speed < 0)
                {
                    if (!(vd->flags & AV_REVERSE_BLOCK_START) && vd->timestamp - m_lastVideoPacketTime < -MIN_VIDEO_DETECT_JUMP_INTERVAL*3)
                    {
                        // I have found avi file where sometimes 290 ms between frames. At reverse mode, bad afterJump affect file very strong
                        afterJump(vd);
                    }
                }
                else
                    afterJump(vd);
            }
            m_lastVideoPacketTime = vd->timestamp;

            if (channel >= CL_MAX_CHANNELS)
                return result;

            // this is the only point to addreff;
            // video data can escape from this object only if displayed or in case of clearVideoQueue
            // so release ref must be in both places

            if (m_singleShotMode && m_singleShotQuantProcessed)
            {
                enqueueVideo(vd);
                return result;
            }
        }
        // three are 3 possible scenarios:

        //1) we do not have audio playing;
        if (!haveAudio(speed))
        {
            qint64 m_videoDuration = m_videoQueue[0].size() * m_lastNonZerroDuration;
            if (vd && m_videoDuration >  1000 * 1000)
            {
                // skip current video packet, process it latter
                result = false;
                vd = QnCompressedVideoDataPtr();
            }
            vd = nextInOutVideodata(vd, channel);
            if (!vd)
                return result; // impossible? incoming vd!=0

            if(m_display[channel] != NULL)
                m_display[channel]->setCurrentTime(AV_NOPTS_VALUE);

            display(vd, !(vd->flags & QnAbstractMediaData::MediaFlags_Ignore), speed);
            m_singleShotQuantProcessed = true;
            return result;
        }

        // no more data expected. play as is
        if (flushCurrentBuffer)
            m_audioDisplay->playCurrentBuffer();

        //2) we have audio and it's buffering( not playing yet )
        if (m_audioDisplay->isBuffering() && !flushCurrentBuffer)
        //if (m_audioDisplay->isBuffering() || m_audioDisplay->msInBuffer() < AUDIO_BUFF_SIZE / 10)
        {
            // audio is not playinf yet; video must not be played as well
            enqueueVideo(vd);
            return result;
        }
        //3) video and audio playing
        else
        {
            // New av sync algorithm required MT decoding
            if (!m_useMtDecoding)
                setMTDecoding(true);

            QnCompressedVideoDataPtr incoming;
            if (m_audioDisplay->msInBuffer() < AUDIO_BUFF_SIZE )
                incoming = vd; // process packet
            else {
                result = false;
            }

            vd = nextInOutVideodata(incoming, channel);
            incoming = QnCompressedVideoDataPtr();
            if (vd) {
                bool ignoreVideo = vd->flags & QnAbstractMediaData::MediaFlags_Ignore;
                if (!ignoreVideo) {
                    QMutexLocker lock(&m_timeMutex);
                    m_lastDecodedTime = vd->timestamp;
                }
                if (m_lastAudioPacketTime != m_syncAudioTime) {
                    qint64 currentAudioTime = m_lastAudioPacketTime - (quint64)m_audioDisplay->msInBuffer()*1000;
                    if(m_display[channel])
                        m_display[channel]->setCurrentTime(currentAudioTime);
                    m_syncAudioTime = m_lastAudioPacketTime; // sync audio time prevent stopping video, if audio track is disapearred
                }                
                display(vd, !ignoreVideo, speed);
                m_singleShotQuantProcessed = true;
            }

        }

        return result;
    }

    return true;
}

void CLCamDisplay::setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val)
{
    if (val == m_lightCpuMode)
        return;

    cl_log.log("slow queue size=", m_tooSlowCounter, cl_logWARNING);
    cl_log.log("set CPUMode=", val, cl_logWARNING);

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_display[i])
            m_display[i]->setLightCPUMode(val);
    }
    m_lightCpuMode = val;
}

void CLCamDisplay::pauseAudio()
{
    m_audioDisplay->suspend();
}

void CLCamDisplay::playAudio(bool play)
{
    if (m_playAudio == play)
        return;

    m_needChangePriority = true;
    m_playAudio = play;
    if (!m_playAudio)
        m_audioDisplay->clearDeviceBuffer();
    else
        m_audioDisplay->resume();
    if (!play)
        setMTDecoding(false);
}

//==========================================================================

bool CLCamDisplay::haveAudio(float speed) const
{
    return m_playAudio && m_hadAudio && qAbs(speed-1.0) < FPS_EPS && !m_ignoringVideo && m_audioDisplay->isFormatSupported();
}

QnCompressedVideoDataPtr CLCamDisplay::nextInOutVideodata(QnCompressedVideoDataPtr incoming, int channel)
{
    if (m_videoQueue[channel].isEmpty())
        return incoming;

    if (incoming)
        enqueueVideo(incoming);

    // queue is not empty
    return m_videoQueue[channel].dequeue();
}

quint64 CLCamDisplay::nextVideoImageTime(QnCompressedVideoDataPtr incoming, int channel) const
{
    if (m_videoQueue[channel].isEmpty())
        return incoming ? incoming->timestamp : 0;

    // queue is not empty
    return m_videoQueue[channel].head()->timestamp;
}

quint64 CLCamDisplay::nextVideoImageTime(int channel) const
{
    if (m_videoQueue[channel].isEmpty())
        return m_lastVideoPacketTime;

    if (m_videoBufferOverflow)
        return 0;

    return m_videoQueue[channel].head()->timestamp;
}

void CLCamDisplay::clearVideoQueue()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        while (!m_videoQueue[i].isEmpty())
            m_videoQueue[i].dequeue();
    }
    m_videoBufferOverflow = false;
}

void CLCamDisplay::enqueueVideo(QnCompressedVideoDataPtr vd)
{
    m_videoQueue[vd->channelNumber].enqueue(vd);
    if (m_videoQueue[vd->channelNumber].size() > 60 * 6) // I assume we are not gonna buffer
    {
        cl_log.log(QLatin1String("Video buffer overflow!"), cl_logWARNING);
        m_videoQueue[vd->channelNumber].dequeue();
        // some protection for very large difference between video and audio tracks. Need to improve sync logic for this case (now a lot of glithces)
        m_videoBufferOverflow = true;
    }
}

void CLCamDisplay::setMTDecoding(bool value)
{
    m_useMtDecoding = value;
    for (int i = 0; i < CL_MAX_CHANNELS; i++)
    {
        if (m_display[i])
            m_display[i]->setMTDecoding(value);
    }
    if (value)
        setSpeed(m_speed); // decoder now faster. reinit speed statistics
    m_realTimeHurryUp = 5;
}

void CLCamDisplay::onRealTimeStreamHint(bool value)
{
    m_isRealTimeSource = value;
    emit liveMode(m_isRealTimeSource);
    if (m_isRealTimeSource)
        m_speed = 1.0f;
}

void CLCamDisplay::onSlowSourceHint()
{
    m_dataQueue.setMaxSize(CL_MAX_DISPLAY_QUEUE_FOR_SLOW_SOURCE_SIZE);
}

qint64 CLCamDisplay::getDisplayedMax() const
{
    qint64 rez = AV_NOPTS_VALUE;
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
    {
        rez = qMax(rez, m_display[i]->getLastDisplayedTime());
    }
    return rez;
}

qint64 CLCamDisplay::getDisplayedMin() const
{
    qint64 rez = m_display[0]->getLastDisplayedTime();
    for (int i = 1; i < CL_MAX_CHANNELS && m_display[i]; ++i)
    {
        qint64 val = m_display[i]->getLastDisplayedTime();
        if (val != AV_NOPTS_VALUE) {
            rez = qMin(rez, val);
        }
    }
    return rez;
}

qint64 CLCamDisplay::getCurrentTime() const 
{
    if (m_display[0]->isTimeBlocked())
        return m_display[0]->getLastDisplayedTime();
    else if (m_speed >= 0)
        return getDisplayedMax();
    else
        return getDisplayedMin();
}

qint64 CLCamDisplay::getMinReverseTime() const
{
    qint64 rez = m_nextReverseTime[0];
    for (int i = 1; i < CL_MAX_CHANNELS && m_display[i]; ++i) 
    {
        if (m_nextReverseTime[i] != AV_NOPTS_VALUE && m_nextReverseTime[i] < rez)
            rez = m_nextReverseTime[i];
    }
    return rez;
}

qint64 CLCamDisplay::getNextTime() const
{
    if (m_display[0]->isTimeBlocked())
        return m_display[0]->getLastDisplayedTime();
    else 
        return m_speed < 0 ? getMinReverseTime() : m_lastDecodedTime;
}

qint64 CLCamDisplay::getDisplayedTime() const
{
    return getCurrentTime();
}

qint64 CLCamDisplay::getExternalTime() const
{
    if (m_extTimeSrc && m_extTimeSrc->isEnabled())
        return m_extTimeSrc->getDisplayedTime();
    else
        return getCurrentTime();
}


void CLCamDisplay::setExternalTimeSource(QnlTimeSource* value) 
{ 
    m_extTimeSrc = value; 
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
        m_display[i]->canUseBufferedFrameDisplayer(m_extTimeSrc == 0);
    }
}

bool CLCamDisplay::isRealTimeSource() const 
{ 
    return m_isRealTimeSource; 
}

bool CLCamDisplay::isStillImage() const
{
    return m_isStillImage;
}

bool CLCamDisplay::isNoData() const
{
    if (isRealTimeSource())
        return false;
    if (!m_extTimeSrc)
        return false;
    if (m_executingJump > 0 || m_executingChangeSpeed || m_buffering)
        return false;

    qint64 ct = m_extTimeSrc->getCurrentTime();
    bool useSync = m_extTimeSrc && m_extTimeSrc->isEnabled() && m_jumpTime != DATETIME_NOW;
    if (!useSync || ct == DATETIME_NOW)
        return false;

    return m_isLongWaiting || m_emptyPacketCounter >= 3;
    /*
    if (m_isLongWaiting)
        return true;

    if (m_emptyPacketCounter >= 3)
        return true;

    int sign = m_speed >= 0 ? 1 : -1;
    return sign *(getCurrentTime() - ct) > MAX_FRAME_DURATION*1000;
    */
}
