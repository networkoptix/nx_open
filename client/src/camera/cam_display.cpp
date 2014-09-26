#include "cam_display.h"

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>

#include <utils/common/log.h>
#include <utils/common/util.h>
#include <utils/common/synctime.h>

#include "core/resource/camera_resource.h"
#include "core/datapacket/media_data_packet.h"

#include "decoders/audio/ffmpeg_audio.h"
#include "plugins/resource/archive/archive_stream_reader.h"
#include "redass/redass_controller.h"

#include "video_stream_display.h"
#include "audio_stream_display.h"


#if defined(Q_OS_MAC)
#include <CoreServices/CoreServices.h>
#elif defined(Q_OS_WIN)
#include <qt_windows.h>
#include <plugins/resource/desktop_win/desktop_resource.h>
#endif


Q_GLOBAL_STATIC(QMutex, activityMutex)
static qint64 activityTime = 0;
static const int REDASS_DELAY_INTERVAL = 2 * 1000*1000ll; // if archive frame delayed for interval, mark stream as slow
static const int LIVE_MEDIA_LEN_THRESHOLD = 400*1000ll;   // do not sleep in live mode if queue is large

static void updateActivity()
{
    QMutexLocker locker(activityMutex());

    if (QDateTime::currentMSecsSinceEpoch() >= activityTime)
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
        activityTime = QDateTime::currentMSecsSinceEpoch() + 20000;
    }
}


// a lot of small audio packets in bluray HD audio codecs. So, previous size 7 is not enought
#define CL_MAX_DISPLAY_QUEUE_SIZE 20
#define CL_MAX_DISPLAY_QUEUE_FOR_SLOW_SOURCE_SIZE 20

static const int DEFAULT_AUDIO_BUFF_SIZE = 1000 * 4;

static const int REALTIME_AUDIO_BUFFER_SIZE = 750; // at ms, max buffer 
static const int REALTIME_AUDIO_PREBUFFER = 75; // at ms, prebuffer 

static const qint64 MIN_VIDEO_DETECT_JUMP_INTERVAL = 300 * 1000; // 300ms
//static const qint64 MIN_AUDIO_DETECT_JUMP_INTERVAL = MIN_VIDEO_DETECT_JUMP_INTERVAL + AUDIO_BUFF_SIZE*1000;
//static const int MAX_VALID_SLEEP_TIME = 1000*1000*5;
static const int MAX_VALID_SLEEP_LIVE_TIME = 1000 * 500; // 5 seconds as most long sleep time
static const int SLOW_COUNTER_THRESHOLD = 24;
static const double FPS_EPS = 0.0001;

static const int DEFAULT_DELAY_OVERDRAFT = 5000 * 1000;

QnCamDisplay::QnCamDisplay(QnMediaResourcePtr resource, QnArchiveStreamReader* reader): 
    QnAbstractDataConsumer(CL_MAX_DISPLAY_QUEUE_SIZE),
    m_audioDisplay(0),
    m_delay(DEFAULT_DELAY_OVERDRAFT), 
    m_speed(1.0),
    m_prevSpeed(1.0),
    m_playAudio(false),
    m_needChangePriority(false),
    m_hadAudio(false),
    m_lastAudioPacketTime(0),
    m_syncAudioTime(AV_NOPTS_VALUE),
    m_totalFrames(0),
    m_fczFrames(0),
    m_iFrames(0),
    m_lastVideoPacketTime(0),
    m_lastDecodedTime(AV_NOPTS_VALUE),
    m_previousVideoTime(0),
    m_lastNonZerroDuration(0),
    m_lastSleepInterval(0),
    m_afterJump(false),
    m_bofReceived(false),
    m_displayLasts(0),
    m_ignoringVideo(false),
    m_isRealTimeSource(true),
    m_videoBufferOverflow(false),
    m_singleShotMode(false),
    m_singleShotQuantProcessed(false),
    m_jumpTime(DATETIME_NOW),
    m_lightCpuMode(QnAbstractVideoDecoder::DecodeMode_Full),
    m_lastFrameDisplayed(QnVideoStreamDisplay::Status_Displayed),
    m_realTimeHurryUp(false),
    m_delayedFrameCount(0),
    m_extTimeSrc(0),
    m_useMtDecoding(false),
    m_buffering(0),
    m_executingJump(0),
    m_skipPrevJumpSignal(0),
    m_processedPackets(0),
    m_emptyPacketCounter(0),
    m_isStillImage(false),
    m_isLongWaiting(false),
    m_skippingFramesStarted(false),
    m_executingChangeSpeed(false),
    m_eofSignalSended(false),
    m_videoQueueDuration(0),
    m_useMTRealTimeDecode(false),
    m_forceMtDecoding(false),
    m_timeMutex(QMutex::Recursive),
    m_resource(resource),
    m_firstAfterJumpTime(AV_NOPTS_VALUE),
    m_receivedInterval(0),
    m_archiveReader(reader),
    m_fullScreen(false),
    m_prevLQ(-1),
    m_doNotChangeDisplayTime(false),
    m_firstLivePacket(true),
    m_multiView(false),
    m_fisheyeEnabled(false)
{

    if (resource && resource->toResource()->hasFlags(Qn::live_cam))
        m_isRealTimeSource = true;
    else
        m_isRealTimeSource = false;

    if (resource && resource->toResource()->hasFlags(Qn::still_image)) {
        m_isStillImage = true;

        QFileInfo fileInfo(resource->toResource()->getUrl());
        if (fileInfo.isReadable())
            resource->toResource()->setStatus(Qn::Online);
        else
            resource->toResource()->setStatus(Qn::Offline);
    }

    m_storedMaxQueueSize = m_dataQueue.maxSize();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        m_display[i] = 0;
        m_nextReverseTime[i] = AV_NOPTS_VALUE;
    }
    int expectedBufferSize = m_isRealTimeSource ? REALTIME_AUDIO_BUFFER_SIZE : DEFAULT_AUDIO_BUFF_SIZE;
    int expectedPrebuferSize = m_isRealTimeSource ? REALTIME_AUDIO_PREBUFFER : DEFAULT_AUDIO_BUFF_SIZE/2;
    setAudioBufferSize(expectedBufferSize, expectedPrebuferSize);

#ifdef Q_OS_WIN
    QnDesktopResourcePtr desktopResource = resource.dynamicCast<QnDesktopResource>();
    if (desktopResource && desktopResource->isRendererSlow())
        m_forceMtDecoding = true; // not enough speed for desktop camera with aero in single thread mode because of slow rendering
#endif
}

QnCamDisplay::~QnCamDisplay()
{
    qnRedAssController->unregisterConsumer(this);

    Q_ASSERT(!isRunning());
    stop();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        delete m_display[i];

    clearVideoQueue();
    delete m_audioDisplay;
}

void QnCamDisplay::setAudioBufferSize(int bufferSize, int prebufferSize)
{
    m_audioBufferSize = bufferSize;
    m_minAudioDetectJumpInterval = MIN_VIDEO_DETECT_JUMP_INTERVAL + m_audioBufferSize*1000;
    QMutexLocker lock(&m_audioChangeMutex);
    delete m_audioDisplay;
    m_audioDisplay = new QnAudioStreamDisplay(m_audioBufferSize, prebufferSize);

}

void QnCamDisplay::pause()
{
    QnAbstractDataConsumer::pause();
    m_isRealTimeSource = false;
    emit liveMode(false);
    QMutexLocker lock(&m_audioChangeMutex);
    m_audioDisplay->suspend();
}

void QnCamDisplay::resume()
{
    m_delay.afterdelay();
    m_singleShotMode = false;
    {
        QMutexLocker lock(&m_audioChangeMutex);
        m_audioDisplay->resume();
    }
    m_firstAfterJumpTime = AV_NOPTS_VALUE;
    QnAbstractDataConsumer::resume();
}

void QnCamDisplay::addVideoRenderer(int channelCount, QnAbstractRenderer* vw, bool canDownscale)
{
    Q_ASSERT(channelCount <= CL_MAX_CHANNELS);
    
    for(int i = 0; i < channelCount; i++)
    {
        if (!m_display[i])
            m_display[i] = new QnVideoStreamDisplay(canDownscale, i);
        int rendersCount = m_display[i]->addRenderer(vw);
        m_multiView = rendersCount > 1;
    }
}

void QnCamDisplay::removeVideoRenderer(QnAbstractRenderer* vw)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        if (m_display[i]) {
            int rendersCount = m_display[i]->removeRenderer(vw);
            m_multiView = rendersCount > 1;
        }
    }
}

QImage QnCamDisplay::getScreenshot(int channel,
                                   const ImageCorrectionParams& params,
                                   const QnMediaDewarpingParams &mediaDewarping,
                                   const QnItemDewarpingParams &itemDewarping,
                                   bool anyQuality)
{
    return m_display[channel]->getScreenshot(params, mediaDewarping, itemDewarping, anyQuality);
}

QImage QnCamDisplay::getGrayscaleScreenshot(int channel)
{
    return m_display[channel]->getGrayscaleScreenshot();
}

QSize QnCamDisplay::getFrameSize(int channel) const {
    return m_display[channel]->getImageSize();
}

void QnCamDisplay::hurryUpCheck(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime)
{
    bool isVideoCamera = vd->dataProvider && qSharedPointerDynamicCast<QnVirtualCameraResource>(m_resource) != 0;
    if (isVideoCamera)
        hurryUpCheckForCamera(vd, speed, needToSleep, realSleepTime);
    else
        hurryUpCheckForLocalFile(vd, speed, needToSleep, realSleepTime);
}

void QnCamDisplay::hurryUpCkeckForCamera2(QnAbstractMediaDataPtr media)
{
    bool isVideoCamera = media->dataProvider && qSharedPointerDynamicCast<QnVirtualCameraResource>(m_resource) != 0;
    if (media->dataType != QnAbstractMediaData::VIDEO && media->dataType != QnAbstractMediaData::AUDIO)
        return;

    if (isVideoCamera)
    {
        //bool isLive = media->flags & QnAbstractMediaData::MediaFlags_LIVE;
        //bool isPrebuffer = media->flags & QnAbstractMediaData::MediaFlags_FCZ;
        if (m_speed < 1.0 || m_singleShotMode)
            return;
        if ((quint64)m_firstAfterJumpTime == AV_NOPTS_VALUE) {
            m_firstAfterJumpTime = media->timestamp;
            m_receivedInterval = 0;
            m_afterJumpTimer.restart();
            return;
        }

        m_receivedInterval = qMax(m_receivedInterval, media->timestamp - m_firstAfterJumpTime);
        if (m_afterJumpTimer.elapsed()*1000 > REDASS_DELAY_INTERVAL)
        {
            if (m_receivedInterval/1000 < m_afterJumpTimer.elapsed()/2) 
            {
                QnArchiveStreamReader* reader = dynamic_cast<QnArchiveStreamReader*> (media->dataProvider);
                qnRedAssController->onSlowStream(reader);
            }
        }
    }
}

QnArchiveStreamReader* QnCamDisplay::getArchiveReader() const {
    return m_archiveReader;
}

void QnCamDisplay::hurryUpCheckForCamera(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime)
{
    Q_UNUSED(needToSleep)
    Q_UNUSED(speed)

    if (vd->flags & QnAbstractMediaData::MediaFlags_LIVE) 
        return;
    if (vd->flags & QnAbstractMediaData::MediaFlags_Ignore)
        return;

    if (m_archiveReader)
    {
        if (realSleepTime <= -REDASS_DELAY_INTERVAL) 
        {
            m_delayedFrameCount = qMax(0, m_delayedFrameCount);
            m_delayedFrameCount++;
            if (m_delayedFrameCount > 10 && m_archiveReader->getQuality() != MEDIA_Quality_Low /*&& canSwitchQuality()*/)
                qnRedAssController->onSlowStream(m_archiveReader);
        }
        else if (realSleepTime >= 0)
        {
            m_delayedFrameCount = qMin(0, m_delayedFrameCount);
            m_delayedFrameCount--;
            if (m_delayedFrameCount < -10 && m_dataQueue.size() >= m_dataQueue.size()*0.75)
                qnRedAssController->streamBackToNormal(m_archiveReader);
        }
    }
}

void QnCamDisplay::hurryUpCheckForLocalFile(QnCompressedVideoDataPtr vd, float speed, qint64 needToSleep, qint64 realSleepTime)
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

int QnCamDisplay::getBufferingMask()
{
    int channelMask = 0;
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) 
        channelMask = channelMask*2  + 1;
    return channelMask;
}

float sign(float value)
{
    if (value == 0)
        return 0;
    return value > 0 ? 1 : -1;
}

bool QnCamDisplay::display(QnCompressedVideoDataPtr vd, bool sleep, float speed)
{
//    cl_log.log("queueSize=", m_dataQueue.size(), cl_logALWAYS);
//    cl_log.log(QDateTime::fromMSecsSinceEpoch(vd->timestamp/1000).toString("hh.mm.ss.zzz"), cl_logALWAYS);

    // simple data provider/streamer/streamreader has the same delay, but who cares ?
    // to avoid cpu usage in case of a lot data in queue(zoomed out on the scene, lets add same delays here )
    quint64 currentTime = vd->timestamp;

    m_totalFrames++;
    if (vd->flags & AV_PKT_FLAG_KEY)
        m_iFrames++;
    bool isPrebuffering = vd->flags & QnAbstractMediaData::MediaFlags_FCZ;
    if (isPrebuffering)
        m_fczFrames++; // fast channel zapping

    // in ideal world data comes to queue at the same speed as it goes out
    // but timer on the sender side runs at a bit different rate in comparison to the timer here
    // adaptive delay will not solve all problems => need to minus little appendix based on queue size
    qint32 needToSleep;

    if ((vd->flags & QnAbstractMediaData::MediaFlags_BOF) || isPrebuffering)
        m_lastSleepInterval = needToSleep = 0;

    if (vd->flags & AV_REVERSE_BLOCK_START)
    {
        const long frameTimeDiff = abs((long)(currentTime - m_previousVideoTime));
        needToSleep = (m_lastSleepInterval == 0) && (frameTimeDiff < MAX_FRAME_DURATION * 1000)
            ? frameTimeDiff
            : m_lastSleepInterval;
    }
    else {
        needToSleep = m_lastSleepInterval = (currentTime - m_previousVideoTime) * 1.0/qAbs(speed);
    }

    //qDebug() << "vd->flags & QnAbstractMediaData::MediaFlags_FCZ" << (vd->flags & QnAbstractMediaData::MediaFlags_FCZ);

    bool canSwitchToMT = isFullScreen() || qnRedAssController->counsumerCount() == 1;
    bool shouldSwitchToMT = (m_isRealTimeSource && m_totalFrames > 100 && m_dataQueue.size() >= m_dataQueue.size()-1) || !m_isRealTimeSource;
    if (canSwitchToMT && shouldSwitchToMT) {
        if (!m_useMTRealTimeDecode) {
            m_useMTRealTimeDecode = true;
            setMTDecoding(true); 
        }
    }
    else {
        if (m_useMTRealTimeDecode) {
            m_totalFrames = 0;
            m_useMTRealTimeDecode = false;
            setMTDecoding(false); 
        }
    }


    if (m_isRealTimeSource)
    {

        if (m_firstLivePacket) {
            m_delay.afterdelay();
            m_delay.addQuant(LIVE_MEDIA_LEN_THRESHOLD/2); // realtime buffering for more smooth playback
            m_firstLivePacket = false;
        }

        QnAbstractMediaDataPtr lastFrame = m_dataQueue.last().dynamicCast<QnAbstractMediaData>();
        if (lastFrame && lastFrame->dataType == QnAbstractMediaData::VIDEO) 
        {
            qint64 queueLen = lastFrame->timestamp - m_lastVideoPacketTime;
            //qDebug() << "queueLen" << queueLen/1000 << "ms";
            if (queueLen > LIVE_MEDIA_LEN_THRESHOLD) {
                sleep = false;
                m_realTimeHurryUp = true;
            }
            else if (m_realTimeHurryUp)
            {
                if (queueLen > LIVE_MEDIA_LEN_THRESHOLD/2)
                    sleep = false;
                else {
                    m_realTimeHurryUp = false;
                    m_delay.afterdelay();
                    m_delay.addQuant(-needToSleep);
                    m_realTimeHurryUp = false;
                }
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

    if (sleep && m_lastFrameDisplayed != QnVideoStreamDisplay::Status_Buffered)
    {
        qint64 realSleepTime = AV_NOPTS_VALUE;
        if (useSync(vd)) 
        {
            qint64 displayedTime = getCurrentTime();
            //bool isSingleShot = vd->flags & QnAbstractMediaData::MediaFlags_SingleShot;
            if (speed != 0  && (quint64)displayedTime != AV_NOPTS_VALUE && m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Displayed &&
                !(vd->flags & QnAbstractMediaData::MediaFlags_BOF))
            {
                Q_ASSERT(!(vd->flags & QnAbstractMediaData::MediaFlags_Ignore));
                //QTime t;
                //t.start();
                int speedSign = speed >= 0 ? 1 : -1;
                bool firstWait = true;
                QTime sleepTimer;
                sleepTimer.start();
                while (!m_afterJump && !m_buffering && !needToStop() && sign(m_speed) == sign(speed) && useSync(vd) && !m_singleShotMode)
                {
                    qint64 ct = m_extTimeSrc->getCurrentTime();
                    qint64 newDisplayedTime = getCurrentTime();
                    if (newDisplayedTime != displayedTime) {
                        // new VideoStreamDisplay update time async! So, currentTime possible may be changed during waiting (it is was not possible before v1.5)
                        displayedTime = newDisplayedTime;
                        firstWait = true;
                    }
                    if (ct != DATETIME_NOW && speedSign *(displayedTime - ct) > 0)
                    {
                        if (firstWait)
                        {
                            m_isLongWaiting = speedSign*(displayedTime - ct) > MAX_FRAME_DURATION*1000;
                            if (m_jumpTime != DATETIME_NOW)
                                m_isLongWaiting &= speedSign*(displayedTime - m_jumpTime)  > MAX_FRAME_DURATION*1000;
                            
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
        else if (!m_display[0]->selfSyncUsed()) {
            if (m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Displayed) {
                qint64 maxSleepTime = needToSleep * 2;
                if (qAbs(speed) > 1.0)
                    maxSleepTime *= speed;
                maxSleepTime = qAbs(maxSleepTime);  // needToSleep OR speed can be negative, but maxSleepTime should always be positive
                if (m_isRealTimeSource)
                    realSleepTime = m_delay.terminatedSleep(needToSleep, maxSleepTime);
                else
                    realSleepTime = m_delay.sleep(needToSleep, maxSleepTime);
            }
            else
                realSleepTime = m_delay.addQuant(needToSleep);
        }
        //qDebug() << "sleep time: " << needToSleep/1000.0 << "  real:" << realSleepTime/1000.0;
        if ((quint64)realSleepTime != AV_NOPTS_VALUE && !m_buffering)
            hurryUpCheck(vd, speed, needToSleep, realSleepTime);
    }

    m_isLongWaiting = false;
    int channel = vd->channelNumber;

    if (m_singleShotMode && m_singleShotQuantProcessed)
        return false;

    QnFrameScaler::DownscaleFactor scaleFactor = QnFrameScaler::factor_any;
    if (channel > 0 && m_display[0]) // if device has more than one channel
    {
        // this here to avoid situation where different channels have different down scale factor; it might lead to diffrent size
        scaleFactor = m_display[0]->getCurrentDownscaleFactor(); // [0] - master channel
    }
    if (vd->flags & QnAbstractMediaData::MediaFlags_StillImage)
        scaleFactor = QnFrameScaler::factor_1; // do not downscale still images


    bool doProcessPacket = true;
    if (m_display[channel])
    {
        // sometimes draw + decoding takes a lot of time. so to be able always sync video and audio we MUST not draw
        QTime displayTime;
        displayTime.restart();

        int isLQ = vd->flags & QnAbstractMediaData::MediaFlags_LowQuality;
        bool qualityChanged = m_prevLQ != -1 && isLQ != m_prevLQ;
        if (qualityChanged && m_speed >= 0)
        {
            m_lastFrameDisplayed = m_display[channel]->flushFrame(vd->channelNumber, scaleFactor);
            if (m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Displayed)
                doProcessPacket = false;
        }
        
        if (doProcessPacket) 
        {
            m_prevLQ = isLQ;

            bool ignoreVideo = vd->flags & QnAbstractMediaData::MediaFlags_Ignore;
            bool draw = !ignoreVideo && (sleep || (m_displayLasts * 1000 < needToSleep)); // do not draw if computer is very slow and we still wanna sync with audio

            if (draw) 
                updateActivity();
            
            if (!(vd->flags & QnAbstractMediaData::MediaFlags_Ignore)) 
            {
                QMutexLocker lock(&m_timeMutex);
                m_lastDecodedTime = vd->timestamp;
            }

            m_lastFrameDisplayed = m_display[channel]->display(vd, draw, scaleFactor);
            if (m_isStillImage && m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Skipped)
                m_eofSignalSended = true;
        }

        if (m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Displayed)
        {
            if (speed < 0)
                m_nextReverseTime[channel] = m_display[channel]->nextReverseTime();

            m_timeMutex.lock();
            if (m_buffering && m_executingJump == 0 && !m_afterJump && !m_skippingFramesStarted)
            {
                m_buffering &= ~(1 << vd->channelNumber);
                m_timeMutex.unlock();
                if (m_buffering == 0) 
                {
                    unblockTimeValue();
                    waitForFramesDisplayed(); // new videoStreamDisplay displays data async, so ensure that new position actual
                    if (m_extTimeSrc)
                        m_extTimeSrc->onBufferingFinished(this);
                }
            }
            else
                m_timeMutex.unlock();
        }

        //if (!ignoreVideo && m_buffering)
        if (m_buffering)
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
    return doProcessPacket;
}

template <class T>
void QnCamDisplay::markIgnoreBefore(const T& queue, qint64 time)
{
    for (int i = 0; i < queue.size(); ++i) {
        QnAbstractMediaDataPtr media = queue.at(i).template dynamicCast<QnAbstractMediaData>();
        if (media) {
            if (m_speed >= 0 && media->timestamp < time)
                media->flags |= QnAbstractMediaData::MediaFlags_Ignore;
            else if (m_speed < 0 && media->timestamp > time)
                media->flags |= QnAbstractMediaData::MediaFlags_Ignore;
        }
    }
}

void QnCamDisplay::onSkippingFrames(qint64 time)
{
    if (m_extTimeSrc)
        m_extTimeSrc->onBufferingStarted(this, time);

    m_dataQueue.lock();
    markIgnoreBefore(m_dataQueue, time);
    m_dataQueue.unlock();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
        markIgnoreBefore(m_videoQueue[i], time);

    QMutexLocker lock(&m_timeMutex);
    m_singleShotQuantProcessed = false;
    m_buffering = getBufferingMask();
    m_skippingFramesStarted = true;

    if (m_speed >= 0)
        blockTimeValue(qMax(time, getCurrentTime()));
    else
        blockTimeValue(qMin(time, getCurrentTime()));

    m_emptyPacketCounter = 0;
    if (m_extTimeSrc && m_eofSignalSended) {
        m_extTimeSrc->onEofReached(this, false);
        m_eofSignalSended = false;
    }
}

void QnCamDisplay::blockTimeValue(qint64 time)
{
    if (!m_doNotChangeDisplayTime) {
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
            m_nextReverseTime[i] = AV_NOPTS_VALUE;
            m_display[i]->blockTimeValue(time);
        }
    }
}

void QnCamDisplay::blockTimeValueSafe(qint64 time)
{
    if (!m_doNotChangeDisplayTime) {
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
            m_nextReverseTime[i] = AV_NOPTS_VALUE;
            m_display[i]->blockTimeValueSafe(time);
        }
    }
}

void QnCamDisplay::waitForFramesDisplayed()
{
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        m_display[i]->waitForFramesDisplayed();
}

void QnCamDisplay::onBeforeJump(qint64 time)
{
    if (m_extTimeSrc)
        m_extTimeSrc->onBufferingStarted(this, m_doNotChangeDisplayTime ? getDisplayedTime() : time);
    /*
    if (time < 1000000ll * 100000)
        cl_log.log("before jump to ", time, cl_logWARNING);
    else
        cl_log.log("before jump to ", QDateTime::fromMSecsSinceEpoch(time/1000).toString(), cl_logWARNING);
    */
    
    QMutexLocker lock(&m_timeMutex);
    onRealTimeStreamHint(time == DATETIME_NOW && m_speed >= 0);

    m_lastDecodedTime = AV_NOPTS_VALUE;
    blockTimeValueSafe(time);
    m_doNotChangeDisplayTime = false;

    m_emptyPacketCounter = 0;
    if (m_extTimeSrc && m_eofSignalSended && time != DATETIME_NOW) {
        m_extTimeSrc->onEofReached(this, false);
        m_eofSignalSended = false;
    }
    clearUnprocessedData();

    if (m_skipPrevJumpSignal > 0) {
        m_skipPrevJumpSignal--;
        return;
    }
    //setSingleShotMode(false);

    
    m_executingJump++;

    m_buffering = getBufferingMask();

    if (m_executingJump > 1)
        clearUnprocessedData();
    m_processedPackets = 0;
}

void QnCamDisplay::onJumpOccured(qint64 time)
{
    /*
    if (time < 1000000ll * 100000)
        cl_log.log("after jump to ", time, cl_logWARNING);
    else
        cl_log.log("after jump to ", QDateTime::fromMSecsSinceEpoch(time/1000).toString(), cl_logWARNING);
    */       
    //if (m_extTimeSrc)
    //    m_extTimeSrc->onBufferingStarted(this, time);

    QMutexLocker lock(&m_timeMutex);
    m_afterJump = true;
    m_bofReceived = false;
    m_buffering = getBufferingMask();
    m_lastDecodedTime = AV_NOPTS_VALUE;
    m_firstAfterJumpTime = AV_NOPTS_VALUE;
    
    m_singleShotQuantProcessed = false;
    m_jumpTime = time;

    if (m_executingJump > 0)
        m_executingJump--;
    m_processedPackets = 0;
    m_delayedFrameCount = 0;
}

void QnCamDisplay::onJumpCanceled(qint64 /*time*/)
{
    m_skipPrevJumpSignal++;
}

void QnCamDisplay::afterJump(QnAbstractMediaDataPtr media)
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
    m_lastFrameDisplayed = QnVideoStreamDisplay::Status_Skipped;
    //m_previousVideoDisplayedTime = 0;
    m_totalFrames = 0;
    m_fczFrames = 0;
    m_iFrames = 0;
    if (!m_afterJump && !m_skippingFramesStarted) // if not more (not handled yet) jumps expected
    {
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
            if (media && !(media->flags & QnAbstractMediaData::MediaFlags_Ignore)) {
                m_display[i]->blockTimeValue(media->timestamp);
            }
            m_nextReverseTime[i] = AV_NOPTS_VALUE;
            m_display[i]->unblockTimeValue();
        }
    }
    m_audioDisplay->clearAudioBuffer();
    m_firstAfterJumpTime = AV_NOPTS_VALUE;
    m_prevLQ = -1;
}

void QnCamDisplay::onReaderPaused()
{
    setSingleShotMode(true);
}

void QnCamDisplay::onReaderResumed()
{
    setSingleShotMode(false);
}

void QnCamDisplay::onPrevFrameOccured()
{
    m_doNotChangeDisplayTime = true; // do not move display time to jump position because jump pos given approximatly
    QMutexLocker lock(&m_audioChangeMutex);
    m_audioDisplay->clearDeviceBuffer();
}

void QnCamDisplay::onNextFrameOccured()
{
    m_singleShotQuantProcessed = false;
    QMutexLocker lock(&m_audioChangeMutex);
    m_audioDisplay->clearDeviceBuffer();
}

void QnCamDisplay::setSingleShotMode(bool single)
{
    m_singleShotMode = single;
    if (m_singleShotMode) {
        m_isRealTimeSource = false;
        emit liveMode(false);
        pauseAudio();
    }
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        m_display[i]->setPausedSafe(single);
}

float QnCamDisplay::getSpeed() const
{
    return m_speed;
}

void QnCamDisplay::setSpeed(float speed)
{
    QMutexLocker lock(&m_timeMutex);
    if (qAbs(speed-m_speed) > FPS_EPS)
    {
        if ((speed >= 0 && m_prevSpeed < 0) || (speed < 0 && m_prevSpeed >= 0)) {
            m_afterJumpTimer.restart();
            m_executingChangeSpeed = true; // do not show "No data" while display preparing for new speed. 
            qint64 time = getExternalTime();
            if (time != DATETIME_NOW)
                blockTimeValue(time);
        }
        if (speed < 0 && m_speed >= 0) {
            for (int i = 0; i < CL_MAX_CHANNELS; ++i)
                m_nextReverseTime[i] = AV_NOPTS_VALUE;
        }
        m_speed = speed;
    }
}

void QnCamDisplay::unblockTimeValue()
{
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        m_display[i]->unblockTimeValue();
}

void QnCamDisplay::processNewSpeed(float speed)
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
    else if (speed != 0) {
        setMTDecoding(m_useMtDecoding);
    }

    if (speed < 0 && m_prevSpeed >= 0)
        m_buffering = getBufferingMask(); // decode first gop is required some time

    if (m_prevSpeed == 0 && m_extTimeSrc && m_eofSignalSended)
    {
        m_extTimeSrc->onEofReached(this, false);
        m_eofSignalSended = false;
    }

    if ((speed >= 0 && m_prevSpeed < 0) || (speed < 0 && m_prevSpeed >= 0))
    {
        //m_dataQueue.clear();
        clearVideoQueue();
        QMutexLocker lock(&m_timeMutex);
        m_lastDecodedTime = AV_NOPTS_VALUE;
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
            m_nextReverseTime[i] = AV_NOPTS_VALUE;
        unblockTimeValue();
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
    for (int i = 0; i < CL_MAX_CHANNELS; ++i) {
        if (m_display[i])
            m_display[i]->setSpeed(speed);
    }
    setLightCPUMode(QnAbstractVideoDecoder::DecodeMode_Full);
    m_executingChangeSpeed = false;
}

bool QnCamDisplay::useSync(QnCompressedVideoDataPtr vd)
{
    //return m_extTimeSrc && !(vd->flags & (QnAbstractMediaData::MediaFlags_LIVE | QnAbstractMediaData::MediaFlags_BOF)) && !m_singleShotMode;
    return m_extTimeSrc && m_extTimeSrc->isEnabled() && !(vd->flags & (QnAbstractMediaData::MediaFlags_LIVE | QnAbstractMediaData::MediaFlags_PlayUnsync));
}

void QnCamDisplay::putData(const QnAbstractDataPacketPtr& data)
{
    QnCompressedVideoDataPtr video = qSharedPointerDynamicCast<QnCompressedVideoData>(data);
    if (video && (video->flags & QnAbstractMediaData::MediaFlags_LIVE) && m_dataQueue.size() > 0 && video->timestamp - m_lastVideoPacketTime > LIVE_MEDIA_LEN_THRESHOLD)
    {
        m_delay.breakSleep();
    }
    QnAbstractDataConsumer::putData(data);
    if (video && m_dataQueue.size() < 2)
        hurryUpCkeckForCamera2(video); // check if slow network
}

bool QnCamDisplay::canAcceptData() const
{
    if (m_isRealTimeSource)
        return QnAbstractDataConsumer::canAcceptData();
    else if (m_processedPackets < m_dataQueue.maxSize())
        return m_dataQueue.size() <= m_processedPackets; // slowdown slightly to improve a lot of seek perfomance
    else 
        return QnAbstractDataConsumer::canAcceptData();
}

bool QnCamDisplay::needBuffering(qint64 vTime) const
{
    
    qint64 aTime = m_audioDisplay->startBufferingTime();
    if (aTime == (qint64)AV_NOPTS_VALUE)
        return false;

    return vTime > aTime;
    //return m_audioDisplay->isBuffering() && !flushCurrentBuffer;
}

bool QnCamDisplay::processData(const QnAbstractDataPacketPtr& data)
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

    m_processedPackets++;

    if (media->dataType != QnAbstractMediaData::EMPTY_DATA)
    {
        bool mediaIsLive = media->flags & QnAbstractMediaData::MediaFlags_LIVE;

        m_timeMutex.lock();
        if (mediaIsLive != m_isRealTimeSource && !m_buffering)
            onRealTimeStreamHint(mediaIsLive && !m_singleShotMode && m_speed > 0);
        m_timeMutex.unlock();
    }

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

    m_timeMutex.lock();
    m_skippingFramesStarted = false;
    m_timeMutex.unlock();

    if (vd)
    {
        m_ignoringVideo = vd->flags & QnAbstractMediaData::MediaFlags_Ignore;
        if (m_lastMetadata[vd->channelNumber] && m_lastMetadata[vd->channelNumber]->containTime(vd->timestamp))
            vd->motion = m_lastMetadata[vd->channelNumber];
    }
    
    bool oldIsStillImage = m_isStillImage;
    m_isStillImage = media->flags & QnAbstractMediaData::MediaFlags_StillImage;
    if(oldIsStillImage != m_isStillImage)
        emit stillImageChanged();

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
        else if (m_jumpTime == DATETIME_NOW && (media->flags & QnAbstractMediaData::MediaFlags_LIVE) && (media->flags & AV_PKT_FLAG_KEY))
            m_bofReceived = true;

        if (!m_bofReceived)
            return true; // jump finished, but old data received

        // Some clips has very low key frame rate. This condition protect audio buffer overflowing and improve seeking for such clips
        if (ad && ad->timestamp < m_jumpTime - m_audioBufferSize/2*1000)
            return true; // skip packet
        // clear everything we can
        m_bofReceived = false;
        {
            QMutexLocker lock(&m_timeMutex);
            if (m_executingJump == 0) 
                m_afterJump = false;
        }
        afterJump(media);
        //cl_log.log("ProcessData 2", QDateTime::fromMSecsSinceEpoch(vd->timestamp/1000).toString("hh:mm:ss.zzz"), cl_logALWAYS);
    }
    else if (media->flags & QnAbstractMediaData::MediaFlags_NewServer)
    {
        afterJump(media);
        if (m_extTimeSrc)
            m_extTimeSrc->reinitTime(AV_NOPTS_VALUE);
    }

    if (ad)
    {
        if (speed < 0) {
            m_lastAudioPacketTime = ad->timestamp;
            return true; // ignore audio packet to prevent after jump detection
        }
    }
    if (vd)
        m_fpsStat.updateFpsStatistics(vd);


    QnEmptyMediaDataPtr emptyData = qSharedPointerDynamicCast<QnEmptyMediaData>(data);

    bool flushCurrentBuffer = false;
    int expectedBufferSize = m_isRealTimeSource ? REALTIME_AUDIO_BUFFER_SIZE : DEFAULT_AUDIO_BUFF_SIZE;
    QnCodecAudioFormat currentAudioFormat;
    bool audioParamsChanged = ad && (m_playingFormat != currentAudioFormat.fromAvStream(ad->context) || m_audioDisplay->getAudioBufferSize() != expectedBufferSize);
    if (((media->flags & QnAbstractMediaData::MediaFlags_AfterEOF) || audioParamsChanged) &&
        m_videoQueue[0].size() > 0)
    {
        // skip data (play current buffer
        flushCurrentBuffer = true;
    }
    else if (emptyData && m_videoQueue[0].size() > 0) {
        flushCurrentBuffer = true;
    }
    else if (media->flags & QnAbstractMediaData::MediaFlags_AfterEOF) 
    {
        if (vd && m_display[vd->channelNumber] )
            m_display[vd->channelNumber]->waitForFramesDisplayed();
        //if (vd || ad)
        //    afterJump(media); // do not reinit time for empty mediaData because there are always 0 or DATE_TIME timing
    }


    if (emptyData && !flushCurrentBuffer)
    {
        //if (speed == 0) 
        //    return true;

        m_emptyPacketCounter++;
        // empty data signal about EOF, or read/network error. So, check counter bofore EOF signaling
        //bool playUnsync = (emptyData->flags & QnAbstractMediaData::MediaFlags_PlayUnsync);
        bool isFillerPacket =  emptyData->timestamp > 0 && emptyData->timestamp < DATETIME_NOW;
        if (m_emptyPacketCounter >= 3 || isFillerPacket)
        {
            bool isLive = emptyData->flags & QnAbstractMediaData::MediaFlags_LIVE;
            bool isVideoCamera = qSharedPointerDynamicCast<QnVirtualCameraResource>(m_resource) != 0;
            if (m_extTimeSrc && !isLive && isVideoCamera && !m_eofSignalSended && !isFillerPacket) {
                m_extTimeSrc->onEofReached(this, true); // jump to live if needed
                m_eofSignalSended = true;
            }

            /*
            // One camera from several sync cameras may reach BOF/EOF
            // move current time position to the edge to prevent other cameras blocking
            m_nextReverseTime = m_lastDecodedTime = emptyData->timestamp;
            for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
                m_display[i]->overrideTimestampOfNextFrameToRender(m_lastDecodedTime);
            }
            */

            //performing before locking m_timeMutex. Otherwise we could get dead-lock between this thread and a setSpeed, called from main thread.
                //E.g. overrideTimestampOfNextFrameToRender waits for frames rendered, setSpeed (main thread) waits for m_timeMutex
            for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
                m_display[i]->flushFramesToRenderer();

            m_timeMutex.lock();
            m_lastDecodedTime = AV_NOPTS_VALUE;
            for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
                if( m_display[i] )
                    m_display[i]->overrideTimestampOfNextFrameToRender(emptyData->timestamp);
                m_nextReverseTime[i] = AV_NOPTS_VALUE;
            }

            if (m_buffering && m_executingJump == 0) 
            {
                m_buffering = 0;
                m_timeMutex.unlock();
                if (m_extTimeSrc)
                    m_extTimeSrc->onBufferingFinished(this);
                unblockTimeValue();
            }
            else 
                m_timeMutex.unlock();
        }
        else {
            QnArchiveStreamReader* archive = dynamic_cast<QnArchiveStreamReader*>(emptyData->dataProvider);
            if (archive && archive->isSingleShotMode())
                archive->needMoreData();
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

    if (ad && !flushCurrentBuffer)
    {
        if (audioParamsChanged)
        {
            int audioPrebufferSize = m_isRealTimeSource ? REALTIME_AUDIO_PREBUFFER : DEFAULT_AUDIO_BUFF_SIZE/2;
            QMutexLocker lock(&m_audioChangeMutex);
            delete m_audioDisplay;
            m_audioBufferSize = expectedBufferSize;
            m_audioDisplay = new QnAudioStreamDisplay(m_audioBufferSize, audioPrebufferSize);
            m_playingFormat = currentAudioFormat;
        }

        // after seek, when audio is shifted related video (it is often), first audio packet will be < seek threshold
        // so, second afterJump is generated after several video packet. To prevent it, increase jump detection interval for audio

        if (ad->timestamp && ad->timestamp - m_lastAudioPacketTime < -m_minAudioDetectJumpInterval)
        {
            afterJump(ad);
        }

        m_lastAudioPacketTime = ad->timestamp;

        // we synch video to the audio; so just put audio in player with out thinking
        if (m_playAudio && qAbs(speed-1.0) < FPS_EPS)
        {
            if (m_audioDisplay->msInBuffer() > m_audioBufferSize)
            {
                bool useSync = m_extTimeSrc && m_extTimeSrc->isEnabled();
                if (m_isRealTimeSource || useSync)
                    return true; // skip data
                else
                    QnSleep::msleep(40); // Audio buffer too large. waiting
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
                        m_buffering = getBufferingMask();
                        afterJump(vd);
                    }
                }
                else {
                    m_buffering = getBufferingMask();
                    afterJump(vd);
                }
            }
            m_lastVideoPacketTime = vd->timestamp;

            if (channel >= CL_MAX_CHANNELS)
                return result;

            // this is the only point to addreff;
            // video data can escape from this object only if displayed or in case of clearVideoQueue
            // so release ref must be in both places

            if (m_singleShotMode && m_singleShotQuantProcessed)
            {
                //enqueueVideo(vd);
                //return result;
                return false;
            }
        }
        // three are 3 possible scenarios:

        //1) we do not have audio playing;
        if (!haveAudio(speed) || isAudioHoleDetected(vd))
        {
            qint64 m_videoDuration = m_videoQueue[0].size() * m_lastNonZerroDuration;
            if (vd && m_videoDuration >  1000 * 1000)
            {
                // skip current video packet, process it latter
                result = false;
                vd = QnCompressedVideoDataPtr();
            }
            QnCompressedVideoDataPtr incoming = vd;
            vd = nextInOutVideodata(vd, channel);
            if (!vd)
                return result; // impossible? incoming vd!=0

            if(m_display[channel] != NULL)
                m_display[channel]->setCurrentTime(AV_NOPTS_VALUE);

            if (!display(vd, !(vd->flags & QnAbstractMediaData::MediaFlags_Ignore), speed)) {
                restoreVideoQueue(incoming, vd, vd->channelNumber);
                return false; // keep frame
            }
            if (m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Displayed && !m_afterJump)
                m_singleShotQuantProcessed = true;
            return result;
        }

        // no more data expected. play as is
        if (flushCurrentBuffer)
            m_audioDisplay->playCurrentBuffer();

        qint64 vTime = nextVideoImageTime(vd, channel);
        //2) we have audio and it's buffering( not playing yet )
        if (!flushCurrentBuffer && needBuffering(vTime))
        //if (m_audioDisplay->isBuffering() || m_audioDisplay->msInBuffer() < m_audioBufferSize / 10)
        {
            // audio is not playinf yet; video must not be played as well
            enqueueVideo(vd);
            return result;
        }
        //3) video and audio playing
        else
        {
            QnCompressedVideoDataPtr incoming;
            if (m_audioDisplay->msInBuffer() < m_audioBufferSize )
                incoming = vd; // process packet
            else {
                result = false;
            }
            vd = nextInOutVideodata(incoming, channel);

            if (vd) {
                if (!m_useMtDecoding && (!m_isRealTimeSource || m_forceMtDecoding))
                    setMTDecoding(true);

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
                if (!display(vd, !ignoreVideo, speed)) {
                    restoreVideoQueue(incoming, vd, vd->channelNumber);
                    return false;  // keep frame
                }
                if (m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Displayed && !m_afterJump)
                    m_singleShotQuantProcessed = true;
            }

        }

        return result;
    }

    return true;
}

void QnCamDisplay::pleaseStop()
{
    QnAbstractDataConsumer::pleaseStop();
    for( int i = 0; i < CL_MAX_CHANNELS; ++i )
        if( m_display[i] )
            m_display[i]->pleaseStop();
}

void QnCamDisplay::setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val)
{
    if (val == m_lightCpuMode)
        return;

    cl_log.log("set CPUMode=", val, cl_logWARNING);

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_display[i])
            m_display[i]->setLightCPUMode(val);
    }
    m_lightCpuMode = val;
}

void QnCamDisplay::playAudio(bool play)
{
    if (m_playAudio == play)
        return;

    if (m_singleShotMode && play)
        return; // ignore audio playing if camDisplay is paused

    m_needChangePriority = true;
    m_playAudio = play;
    {
        QMutexLocker lock(&m_audioChangeMutex);
        if (!m_playAudio)
            m_audioDisplay->clearDeviceBuffer();
        else
            m_audioDisplay->resume();
    }
    if (m_isRealTimeSource)
        setMTDecoding(play && (m_useMTRealTimeDecode || m_forceMtDecoding));
    else
        setMTDecoding(play);
}

void QnCamDisplay::pauseAudio()
{
    m_playAudio = false;
    {
        QMutexLocker lock(&m_audioChangeMutex);
        m_audioDisplay->suspend();
    }
    setMTDecoding(false);
}

//==========================================================================

bool QnCamDisplay::haveAudio(float speed) const
{
    return m_playAudio && m_hadAudio && qAbs(speed-1.0) < FPS_EPS && !m_ignoringVideo && m_audioDisplay->isFormatSupported();
}

QnCompressedVideoDataPtr QnCamDisplay::nextInOutVideodata(QnCompressedVideoDataPtr incoming, int channel)
{
    if (m_videoQueue[channel].isEmpty())
        return incoming;

    if (incoming)
        enqueueVideo(incoming);

    // queue is not empty
    return dequeueVideo(channel);
}

void QnCamDisplay::restoreVideoQueue(QnCompressedVideoDataPtr incoming, QnCompressedVideoDataPtr vd, int channel)
{
    if (!vd || vd == incoming)
        return;
    if (vd)
        m_videoQueue[channel].insert(0, vd);
    if (incoming)
        m_videoQueue->removeAt(m_videoQueue->size()-1);
}


quint64 QnCamDisplay::nextVideoImageTime(QnCompressedVideoDataPtr incoming, int channel) const
{
    if (m_videoQueue[channel].isEmpty())
        return incoming ? incoming->timestamp : 0;

    // queue is not empty
    return m_videoQueue[channel].head()->timestamp;
}

quint64 QnCamDisplay::nextVideoImageTime(int channel) const
{
    if (m_videoQueue[channel].isEmpty())
        return m_lastVideoPacketTime;

    if (m_videoBufferOverflow)
        return 0;

    return m_videoQueue[channel].head()->timestamp;
}

void QnCamDisplay::clearVideoQueue()
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        while (!m_videoQueue[i].isEmpty())
            m_videoQueue[i].dequeue();
    }
    m_videoBufferOverflow = false;
    m_videoQueueDuration = 0;
}

bool QnCamDisplay::isAudioHoleDetected(QnCompressedVideoDataPtr vd)
{
    if (!vd)
        return false;
    bool isVideoCamera = qSharedPointerDynamicCast<QnVirtualCameraResource>(m_resource) != 0;
    if (!isVideoCamera)
        return false; // do not change behaviour for local files
    if (m_videoQueue->isEmpty())
        return false;
    //return m_videoQueue->last()->timestamp - m_videoQueue->first()->timestamp >= MAX_FRAME_DURATION*1000ll;
    return m_videoQueueDuration > m_audioDisplay->getAudioBufferSize() * 2 * 1000;
}

QnCompressedVideoDataPtr QnCamDisplay::dequeueVideo(int channel)
{
    if (m_videoQueue[channel].size() > 1) {
        qint64 timeDiff = m_videoQueue[channel].at(1)->timestamp - m_videoQueue[channel].front()->timestamp;
        if (timeDiff <= MAX_FRAME_DURATION*1000ll) // ignore data holes
            m_videoQueueDuration -= timeDiff;
    }
    return m_videoQueue[channel].dequeue();
}

void QnCamDisplay::enqueueVideo(QnCompressedVideoDataPtr vd)
{
    if (!m_videoQueue[vd->channelNumber].isEmpty()) {
        qint64 timeDiff = vd->timestamp - m_videoQueue[vd->channelNumber].last()->timestamp;
        if (timeDiff <= MAX_FRAME_DURATION*1000ll) // ignore data holes
            m_videoQueueDuration += timeDiff; 
    }
    m_videoQueue[vd->channelNumber].enqueue(vd);
    if (m_videoQueue[vd->channelNumber].size() > 60 * 6) // I assume we are not gonna buffer
    {
        cl_log.log(lit("Video buffer overflow!"), cl_logWARNING);
        dequeueVideo(vd->channelNumber);
        // some protection for very large difference between video and audio tracks. Need to improve sync logic for this case (now a lot of glithces)
        m_videoBufferOverflow = true;
    }
}

void QnCamDisplay::setMTDecoding(bool value)
{
    if (m_useMtDecoding != value)
    {
        m_useMtDecoding = value;
        for (int i = 0; i < CL_MAX_CHANNELS; i++)
        {
            if (m_display[i])
                m_display[i]->setMTDecoding(value);
        }
        //if (value)
        //    setSpeed(m_speed); // decoder now faster. reinit speed statistics
        m_realTimeHurryUp = true;
    }
}

void QnCamDisplay::onRealTimeStreamHint(bool value)
{
    if (value == m_isRealTimeSource)
        return;
    m_isRealTimeSource = value;
    if (m_isRealTimeSource) {
        m_firstLivePacket = true;
        QnResourceConsumer* archive = dynamic_cast<QnResourceConsumer*>(sender());
        if (archive) {
            QnVirtualCameraResourcePtr camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(archive->getResource());
            if (camera)
                m_hadAudio = camera->isAudioEnabled();
            else
                m_isRealTimeSource = false; // realtime mode allowed for cameras only
        }
        setMTDecoding(m_playAudio && m_useMTRealTimeDecode);
    }
    emit liveMode(m_isRealTimeSource);
    if (m_isRealTimeSource && m_speed > 1)
        m_speed = 1.0f;
}

void QnCamDisplay::onSlowSourceHint()
{
    m_dataQueue.setMaxSize(CL_MAX_DISPLAY_QUEUE_FOR_SLOW_SOURCE_SIZE);
}

qint64 QnCamDisplay::getDisplayedMax() const
{
    qint64 rez = AV_NOPTS_VALUE;
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
    {
        rez = qMax(rez, m_display[i]->getTimestampOfNextFrameToRender());
    }
    return rez;
}

qint64 QnCamDisplay::getDisplayedMin() const
{
    qint64 rez = m_display[0]->getTimestampOfNextFrameToRender();
    for (int i = 1; i < CL_MAX_CHANNELS && m_display[i]; ++i)
    {
        qint64 val = m_display[i]->getTimestampOfNextFrameToRender();
        if ((quint64)val != AV_NOPTS_VALUE) {
            rez = qMin(rez, val);
        }
    }
    return rez;
}

qint64 QnCamDisplay::getCurrentTime() const 
{
    if (m_display[0] && m_display[0]->isTimeBlocked())
        return m_display[0]->getTimestampOfNextFrameToRender();
    else if (m_speed >= 0)
        return getDisplayedMax();
    else
        return getDisplayedMin();
}

qint64 QnCamDisplay::getMinReverseTime() const
{
    qint64 rez = m_nextReverseTime[0];
    for (int i = 1; i < CL_MAX_CHANNELS && m_display[i]; ++i) 
    {
        if ((quint64)m_nextReverseTime[i] != AV_NOPTS_VALUE && m_nextReverseTime[i] < rez)
            rez = m_nextReverseTime[i];
    }
    return rez;
}

qint64 QnCamDisplay::getNextTime() const
{
    if( m_display[0] && m_display[0]->isTimeBlocked() )
    {
        return m_display[0]->getTimestampOfNextFrameToRender();
    }
    else
    {
        qint64 rez = m_speed < 0 ? getMinReverseTime() : m_lastDecodedTime;
        qint64 lastTime = m_display[0]->getTimestampOfNextFrameToRender();

        if ((quint64)rez != AV_NOPTS_VALUE)
            return m_speed < 0 ? qMin(rez, lastTime) : qMax(rez, lastTime);
        else
            return lastTime;

        /*
        return m_display[0] == NULL || (quint64)rez != AV_NOPTS_VALUE
            ? rez
            : m_display[0]->getTimestampOfNextFrameToRender();
        */
    }
}

qint64 QnCamDisplay::getDisplayedTime() const
{
    return getCurrentTime();
}

qint64 QnCamDisplay::getExternalTime() const
{
    if (m_extTimeSrc && m_extTimeSrc->isEnabled())
        return m_extTimeSrc->getDisplayedTime();
    else
        return getCurrentTime();
}


void QnCamDisplay::setExternalTimeSource(QnlTimeSource* value) 
{ 
    m_extTimeSrc = value; 
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
        m_display[i]->canUseBufferedFrameDisplayer(m_extTimeSrc == 0);
    }
}

bool QnCamDisplay::isRealTimeSource() const 
{ 
    return m_isRealTimeSource;
}

bool QnCamDisplay::isStillImage() const
{
    return m_isStillImage;
}

bool QnCamDisplay::isEOFReached() const
{
    return m_eofSignalSended == true;
}

bool QnCamDisplay::isLongWaiting() const
{
    if (isRealTimeSource())
        return false;

    if (m_executingJump > 0 || m_executingChangeSpeed || m_buffering)
        return false;

    return m_isLongWaiting || m_emptyPacketCounter >= 3;
}

QSize QnCamDisplay::getMaxScreenSize() const
{
    if (m_display[0])
        return m_display[0]->getMaxScreenSize();
    else
        return QSize();
}

QSize QnCamDisplay::getVideoSize() const
{
    if (m_display[0])
        return m_display[0]->getImageSize();
    else
        return QSize();
}

bool QnCamDisplay::isZoomWindow() const
{
    return m_multiView;
}

bool QnCamDisplay::isFullScreen() const
{
    return m_fullScreen;
}

void QnCamDisplay::setFullScreen(bool fullScreen)
{
    m_fullScreen = fullScreen;
}

bool QnCamDisplay::isFisheyeEnabled() const
{
    return m_fisheyeEnabled;
}

void QnCamDisplay::setFisheyeEnabled(bool fisheyeEnabled)
{
    m_fisheyeEnabled = fisheyeEnabled;
}

int QnCamDisplay::getAvarageFps() const
{
    return m_fpsStat.getFps();
}

bool QnCamDisplay::isBuffering() const
{
    if (m_buffering == 0)
        return false;
    // for offline resource at LIVE position no any data. Check it
    if (!isRealTimeSource())
        return true; // if archive position then buffering mark should be resetted event for offline resource
    return m_resource->toResource()->getStatus() == Qn::Online || m_resource->toResource()->getStatus() == Qn::Recording;
}

qreal QnCamDisplay::overridenAspectRatio() const {
    if (m_display[0])
        return m_display[0]->overridenAspectRatio();
    return 0.0;
}

void QnCamDisplay::setOverridenAspectRatio(qreal aspectRatio) {
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
        m_display[i]->setOverridenAspectRatio(aspectRatio);
    }
}

// -------------------------------- QnFpsStatistics -----------------------

void QnFpsStatistics::updateFpsStatistics(QnCompressedVideoDataPtr vd)
{
    QMutexLocker lock(&m_mutex);
    if ((vd->flags & QnAbstractMediaData::MediaFlags_BOF) || (vd->flags & QnAbstractMediaData::MediaFlags_AfterDrop)) {
        m_lastTime = AV_NOPTS_VALUE;
        return;
    }
    if (m_lastTime != (qint64)AV_NOPTS_VALUE)
    {
        qint64 diff = qAbs(vd->timestamp - m_lastTime);
        if (m_queue.size() >= MAX_QUEUE_SIZE) {
            qint64 oldVal;
            m_queue.pop(oldVal);
            m_queueSum -= oldVal;
        }
        m_queue.push(diff);
        m_queueSum += diff;
    }
    m_lastTime = vd->timestamp;
}

int QnFpsStatistics::getFps() const
{
    QMutexLocker lock(&m_mutex);
    if (m_queue.size() > 0)
        return 1000000.0 / (m_queueSum / (qreal) m_queue.size()) + 0.5;
    else
        return 0;
}
