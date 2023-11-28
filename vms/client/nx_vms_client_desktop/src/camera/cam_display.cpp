// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "cam_display.h"

#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>

#include <client/client_settings.h>
#include <core/resource/camera_resource.h>
#include <core/resource/client_camera.h>
#include <nx/branding.h>
#include <nx/fusion/model_functions.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/config.h>
#include <nx/streaming/media_data_packet.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/ini.h>
#include <nx/vms/client/desktop/radass/radass_controller.h>
#include <utils/common/synctime.h>
#include <utils/common/util.h>

#include "audio_stream_display.h"
#include "video_stream_display.h"

#if defined(Q_OS_MAC)
    #include <IOKit/pwr_mgt/IOPMLib.h>
#elif defined(Q_OS_WIN)
    #include <qt_windows.h>
    #include <plugins/resource/desktop_win/desktop_resource.h>
#endif

using namespace nx::vms::client::desktop;

Q_GLOBAL_STATIC(nx::Mutex, activityMutex)
static qint64 activityTime = 0;
static const int REDASS_DELAY_INTERVAL = 2 * 1000 * 1000ll; // if archive frame delayed for interval, mark stream as slow
static const int REALTIME_AUDIO_PREBUFFER = 75; // at ms, prebuffer
static const std::chrono::milliseconds kMediaMessageDelay(2500);

static void updateActivity()
{
    NX_MUTEX_LOCKER locker( activityMutex() );

    if (QDateTime::currentMSecsSinceEpoch() >= activityTime)
    {
#ifdef Q_OS_MAC
        static IOPMAssertionID powerAssertionID = 0;

        // Prevent computer entering sleep mode unless it disabled in the local settings.
        if (qnSettings->allowComputerEnteringSleepMode())
        {
            if (powerAssertionID == 0)
                return;

            IOPMAssertionRelease(powerAssertionID);
            powerAssertionID = 0;
        }
        else
        {
            if (powerAssertionID != 0)
                return;

            const auto activityReason = QnCamDisplay::tr("%1 Running",
                "%1 will be substituted with desktop client display name, e.g 'NX Witness Client'")
                    .arg(nx::branding::desktopClientDisplayName());

            IOPMAssertionCreateWithName(
                kIOPMAssertionTypeNoDisplaySleep,
                kIOPMAssertionLevelOn,
                activityReason.toCFString(),
                &powerAssertionID);
        }
#elif defined(Q_OS_WIN)
        // disable screen saver ### should we enable it back on exit?
        static bool screenSaverDisabled = SystemParametersInfo(SPI_SETSCREENSAVEACTIVE, FALSE, 0, SPIF_SENDWININICHANGE);
        Q_UNUSED(screenSaverDisabled)

        // Prevent computer entering sleep mode unless it disabled in the local settings.
        if (qnSettings->allowComputerEnteringSleepMode())
        {
            SetThreadExecutionState(ES_CONTINUOUS);
        }
        else
        {
            SetThreadExecutionState(QSysInfo::windowsVersion() < QSysInfo::WV_VISTA
                ? ES_USER_PRESENT | ES_CONTINUOUS
                : ES_AWAYMODE_REQUIRED | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED | ES_CONTINUOUS);
        }
#endif
        // Update system activity timer once per 20 seconds.
        activityTime = QDateTime::currentMSecsSinceEpoch() + 20000;
    }
}

// A lot of small audio packets in Blu-ray HD audio codecs. So, previous size 7 is not enough.
static const int CL_MAX_DISPLAY_QUEUE_SIZE = 20;
static const int CL_MAX_DISPLAY_QUEUE_FOR_SLOW_SOURCE_SIZE = 30;

static const int DEFAULT_AUDIO_BUFF_SIZE = 1000 * 4;

static const int REALTIME_AUDIO_BUFFER_SIZE = 750; // at ms, max buffer

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
    m_iFrames(0),
    m_lastVideoPacketTime(0),
    m_lastDecodedTime(AV_NOPTS_VALUE),
    m_previousVideoTime(0),
    m_lastNonZerroDuration(0),
    m_lastSleepInterval(0),
    m_afterJump(false),
    m_bofReceived(false),
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
    m_processedPackets(0),
    m_emptyPacketCounter(0),
    m_isStillImage(false),
    m_isLongWaiting(false),
    m_skippingFramesTime(AV_NOPTS_VALUE),
    m_executingChangeSpeed(false),
    m_eofSignalSent(false),
    m_videoQueueDuration(0),
    m_useMTRealTimeDecode(false),
    m_forceMtDecoding(false),
    m_timeMutex(nx::Mutex::Recursive),
    m_resource(resource),
    m_firstAfterJumpTime(AV_NOPTS_VALUE),
    m_receivedInterval(0),
    m_archiveReader(reader),
    m_fullScreen(false),
    m_prevLQ(-1),
    m_doNotChangeDisplayTime(false),
    m_multiView(false),
    m_fisheyeEnabled(false),
    m_channelsCount(0),
    m_lastQueuedVideoTime(AV_NOPTS_VALUE),
    m_liveBufferSizeMkSec(initialLiveBufferMkSecs()),
    m_liveMaxLenReached(false),
    m_hasVideo(true)
{
    nx::vms::client::desktop::ini().reload();

    if (resource && resource->toResource()->hasFlags(Qn::live_cam))
        m_isRealTimeSource = true;
    else
        m_isRealTimeSource = false;

    if (resource && resource->toResource()->hasFlags(Qn::still_image))
        m_isStillImage = true;

    m_storedMaxQueueSize = m_dataQueue.maxSize();
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        m_display[i] = 0;
        m_nextReverseTime[i] = AV_NOPTS_VALUE;
    }
    int expectedBufferSize = m_isRealTimeSource ? REALTIME_AUDIO_BUFFER_SIZE : DEFAULT_AUDIO_BUFF_SIZE;
    int expectedPrebuferSize = m_isRealTimeSource ? REALTIME_AUDIO_PREBUFFER : DEFAULT_AUDIO_BUFF_SIZE/2;
    setAudioBufferSize(expectedBufferSize, expectedPrebuferSize);

    if (auto clientCam = resource.dynamicCast<QnClientCameraResource>())
    {
        connect(clientCam.data(), &QnClientCameraResource::dataDropped, this,
            [this] { appContext()->radassController()->onSlowStream(this); });
    }

#ifdef Q_OS_WIN
    QnDesktopResourcePtr desktopResource = resource.dynamicCast<QnDesktopResource>();
    if (desktopResource && desktopResource->isRendererSlow())
        m_useMtDecoding = m_forceMtDecoding = true; // not enough speed for desktop camera with aero in single thread mode because of slow rendering
#endif
    QnSecurityCamResourcePtr camera = resource.dynamicCast<QnSecurityCamResource>();
    if (camera && camera->getMaxFps() >= 50)
        m_useMtDecoding = m_forceMtDecoding = true; // we can get render speed limit instead. MT decoding and displaying frame queue turn on simultaneously

    if (camera && camera->hasFlags(Qn::virtual_camera))
    {
        auto handler = [this] { playAudio(m_shouldPlayAudio); };
        connect(camera.data(), &QnSecurityCamResource::audioEnabledChanged, this, handler);
    }

    if (camera)
    {
        connect(camera.get(), &QnResource::propertyChanged, this,
            [this] (const QnResourcePtr& , const QString& propertyName)
            {
                if (m_streamsChangedCallback)
                {
                    if (propertyName == ResourcePropertyKey::kMediaCapabilities
                        || propertyName == ResourcePropertyKey::kHasDualStreaming
                        || propertyName == ResourcePropertyKey::kStreamUrls)
                    {
                        m_streamsChangedCallback();
                    }
                }
            });

        connect(camera.get(), &QnSecurityCamResource::disableDualStreamingChanged, this,
            [this] { if (m_streamsChangedCallback) m_streamsChangedCallback(); });

    }
}

QnCamDisplay::~QnCamDisplay()
{
    if (auto clientCam = m_resource.dynamicCast<QnClientCameraResource>())
        clientCam->disconnect(this); // disconnecting radassController()->onSlowStream.

    appContext()->radassController()->unregisterConsumer(this);

    NX_ASSERT(!isRunning());
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
    NX_MUTEX_LOCKER lock( &m_audioChangeMutex );
    delete m_audioDisplay;
    m_audioDisplay = new QnAudioStreamDisplay(m_audioBufferSize, prebufferSize);

}

void QnCamDisplay::addVideoRenderer(int channelCount, QnResourceWidgetRenderer* vw, bool canDownscale)
{
    NX_ASSERT(channelCount <= CL_MAX_CHANNELS);

    for(int i = 0; i < channelCount; i++)
    {
        if (!m_display[i])
        {
            m_display[i] = new QnVideoStreamDisplay(m_resource, canDownscale, i);
            m_channelsCount++;
            if (m_forceMtDecoding)
                m_display[i]->setMTDecoding(true);
        }
        int rendersCount = m_display[i]->addRenderer(vw);
        m_multiView = rendersCount > 1;
        if (!isRunning())
            m_display[i]->updateRenderList();
    }
}

int QnCamDisplay::channelsCount() const
{
    return m_channelsCount;
}

void QnCamDisplay::removeVideoRenderer(QnResourceWidgetRenderer* vw)
{
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_display[i])
        {
            int rendersCount = m_display[i]->removeRenderer(vw);
            m_multiView = rendersCount > 1;
        }
    }
}

void QnCamDisplay::addMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    NX_MUTEX_LOCKER lock(&m_metadataConsumersHashMutex);
    m_metadataConsumerByType.insert(metadataConsumer->metadataType(), metadataConsumer);
}

void QnCamDisplay::removeMetadataConsumer(
    const nx::media::AbstractMetadataConsumerPtr& metadataConsumer)
{
    NX_MUTEX_LOCKER lock(&m_metadataConsumersHashMutex);
    m_metadataConsumerByType.remove(metadataConsumer->metadataType(), metadataConsumer);
}

QImage QnCamDisplay::getScreenshot(const QnLegacyTranscodingSettings& imageProcessingParams,
    bool anyQuality)
{
    auto filters = QnImageFilterHelper::createFilterChain(imageProcessingParams);
    CLVideoDecoderOutputPtr frame;
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_display[i])
        {
            frame = m_display[i]->getScreenshot(anyQuality);
            if (!frame)
                continue;

            if (!filters.isTranscodingRequired())
                return frame->toImage();

            if (!filters.isReady())
            {
                filters.prepare(QSize(frame->width, frame->height));
            }
            frame = filters.apply(frame);
        }
    }
    return frame ? frame->toImage() : QImage();
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
        if (m_firstAfterJumpTime == AV_NOPTS_VALUE) {
            m_firstAfterJumpTime = media->timestamp;
            m_receivedInterval = 0;
            m_afterJumpTimer.restart();
            return;
        }

        m_receivedInterval = qMax(m_receivedInterval, media->timestamp - m_firstAfterJumpTime);
        if (m_afterJumpTimer.elapsed()*1000 > REDASS_DELAY_INTERVAL)
        {
            if (m_receivedInterval/1000 < m_afterJumpTimer.elapsed()/2)
                appContext()->radassController()->onSlowStream(this);
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
                appContext()->radassController()->onSlowStream(this);
        }
        else if (realSleepTime >= 0)
        {
            m_delayedFrameCount = qMin(0, m_delayedFrameCount);
            m_delayedFrameCount--;
            if (m_delayedFrameCount < -10 && m_dataQueue.size() >= m_dataQueue.size()*0.75)
            {
                appContext()->radassController()->streamBackToNormal(this);
            }
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

            else if (m_iFrames > 1)
            {
                qint64 avgGopDuration = ((qint64)needToSleep * m_totalFrames)/m_iFrames;
                if (realSleepTime < qMin(-400*1000ll, -avgGopDuration))
                {
                    ; //setLightCPUMode(QnAbstractVideoDecoder::DecodeMode_Fastest);
                }
                else if (vd->flags & AV_PKT_FLAG_KEY)
                    setLightCPUMode(QnAbstractVideoDecoder::DecodeMode_Fast);
            }
        }
        else if (vd->flags & AV_PKT_FLAG_KEY)
        {
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

qint64 QnCamDisplay::doSmartSleep(const qint64 needToSleep, float speed)
{
    qint64 maxSleepTime = needToSleep * 2;
    if (qAbs(speed) > 1.0)
        maxSleepTime *= speed;
    maxSleepTime = qAbs(maxSleepTime);  // needToSleep OR speed can be negative, but maxSleepTime should always be positive
    if (m_isRealTimeSource)
        return m_delay.terminatedSleep(needToSleep, maxSleepTime);
    else
        return m_delay.sleep(needToSleep, maxSleepTime);
}

int QnCamDisplay::maxDataQueueSize(QueueSizeType type) const
{
    if (type == QueueSizeType::slowStream)
        return CL_MAX_DISPLAY_QUEUE_FOR_SLOW_SOURCE_SIZE;

    return CL_MAX_DISPLAY_QUEUE_SIZE;
}

bool QnCamDisplay::useRealTimeHurryUp() const
{
    auto camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(m_resource);
    return m_isRealTimeSource
        || (camera
            && camera->hasCameraCapabilities(nx::vms::api::DeviceCapability::synchronousChannel));
}

bool QnCamDisplay::display(QnCompressedVideoDataPtr vd, bool sleep, float speed)
{
    // simple data provider/streamer/streamreader has the same delay, but who cares ?
    // to avoid cpu usage in case of a lot data in queue(zoomed out on the scene, lets add same delays here )
    quint64 currentTime = vd->timestamp;

    m_totalFrames++;
    if (vd->flags & AV_PKT_FLAG_KEY)
        m_iFrames++;
    bool isPrebuffering = vd->flags & QnAbstractMediaData::MediaFlags_FCZ;

    // in ideal world data comes to queue at the same speed as it goes out
    // but timer on the sender side runs at a bit different rate in comparison to the timer here
    // adaptive delay will not solve all problems => need to minus little appendix based on queue size
    qint32 needToSleep;

    if (isPrebuffering)
        m_lastSleepInterval = 0;

    if (vd->flags & QnAbstractMediaData::MediaFlags_ReverseBlockStart)
    {
        const long frameTimeDiff = abs((long)(currentTime - m_previousVideoTime));
        needToSleep = (m_lastSleepInterval == 0) && (frameTimeDiff < MAX_FRAME_DURATION_MS * 1000)
            ? frameTimeDiff
            : m_lastSleepInterval;
    }
    else
    {
        needToSleep = m_lastSleepInterval = (currentTime - m_previousVideoTime) * 1.0/qAbs(speed);
    }

    //qDebug() << "vd->flags & QnAbstractMediaData::MediaFlags_FCZ" << (vd->flags & QnAbstractMediaData::MediaFlags_FCZ);

    if (!m_forceMtDecoding)
    {
        bool canSwitchToMT = isFullScreen()
            || appContext()->radassController()->consumerCount() == 1;

        bool shouldSwitchToMT = (m_isRealTimeSource && m_totalFrames > 100 && m_dataQueue.size() >= m_dataQueue.size()-1) || !m_isRealTimeSource;
        if (canSwitchToMT && shouldSwitchToMT)
        {
            if (!m_useMTRealTimeDecode)
            {
                m_useMTRealTimeDecode = true;
                setMTDecoding(true);
            }
        }
        else
        {
            if (m_useMTRealTimeDecode)
            {
                m_totalFrames = 0;
                m_useMTRealTimeDecode = false;
                setMTDecoding(false);
            }
        }
    }

    if (useRealTimeHurryUp() && vd && !isPrebuffering)
    {
        qint64 queueLen = m_lastQueuedVideoTime - m_lastVideoPacketTime;
        //qDebug() << "queueLen" << queueLen/1000 << "ms";

        if (queueLen <= 0)
        {
            // This function is used for LIVE mode and archive playback with external synchronization
            // like NVR. Don't increase buffer for archive mode to make item synchronization more
            /// precise.
            if (m_liveMaxLenReached && vd->flags.testFlag(QnAbstractMediaData::MediaFlags_LIVE))
                m_liveBufferSizeMkSec = qMin(maximumLiveBufferMkSecs(), m_liveBufferSizeMkSec * 1.2);
            m_liveMaxLenReached = false;
            NX_VERBOSE(this, "Increase live buffer size to %1 ms because of empty input buffer",
                m_liveBufferSizeMkSec / 1000);
            m_delay.afterdelay();
            m_delay.addQuant(m_liveBufferSizeMkSec /2); // realtime buffering for more smooth playback
            m_realTimeHurryUp = false;
        }
        else if (queueLen > m_liveBufferSizeMkSec)
        {
            m_liveMaxLenReached = true;
            if (!m_realTimeHurryUp)
                NX_VERBOSE(this, "Video queue overflow. Turn no-delay mode on");
            m_realTimeHurryUp = true;
        }
        else if (m_realTimeHurryUp
            && queueLen <= m_liveBufferSizeMkSec / 2)
        {
            NX_VERBOSE(this, "Half video buffer again. Turn no-delay mode off");
            m_realTimeHurryUp = false;
            m_delay.afterdelay();
            m_delay.addQuant(-needToSleep);
        }
        if (m_realTimeHurryUp)
            sleep = false;
    }

    m_previousVideoTime = currentTime;

    // ===== to avoid unrelated streams / stop play delays
    if (needToSleep < 0)
        needToSleep = 0;

    if (needToSleep > MAX_VALID_SLEEP_TIME) // in case of key frame only and sliding archive slider forward - would not look good; need to do smth
        needToSleep = 0;
    // =========

    if (sleep && m_lastFrameDisplayed != QnVideoStreamDisplay::Status_Buffered)
    {
        qint64 realSleepTime = AV_NOPTS_VALUE;
        if (useSync(vd))
        {
            qint64 displayedTime = getCurrentTime();
            //bool isSingleShot = vd->flags & QnAbstractMediaData::MediaFlags_SingleShot;
            if (speed != 0  && displayedTime != AV_NOPTS_VALUE && m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Displayed &&
                !(vd->flags & QnAbstractMediaData::MediaFlags_BOF))
            {
                NX_ASSERT(!(vd->flags & QnAbstractMediaData::MediaFlags_Ignore));
                int speedSign = speed >= 0 ? 1 : -1;
                bool firstWait = true;
                QElapsedTimer sleepTimer;
                sleepTimer.start();
                bool isAudioSuspended = false;
                while (!m_afterJump && !m_buffering && !needToStop() && sign(m_speed) == sign(speed) && useSync(vd) && !m_singleShotMode)
                {
                    qint64 ct = m_extTimeSrc ? m_extTimeSrc->getCurrentTime() : displayedTime;
                    qint64 newDisplayedTime = getCurrentTime();
                    if (newDisplayedTime != displayedTime)
                    {
                        // new VideoStreamDisplay update time async! So, currentTime possible may be changed during waiting (it is was not possible before v1.5)
                        displayedTime = newDisplayedTime;
                        firstWait = true;
                    }
                    bool doDelayForAudio = m_playAudio
                        && m_audioDisplay->isPlaying()
                        && displayedTime > m_audioDisplay->getCurrentTime()
                        && m_audioDisplay->msInBuffer() > 0; //< Audio is ahead video.
                    if ((ct != DATETIME_NOW && speedSign * (displayedTime - ct) > 0)
                        || doDelayForAudio)
                    {
                        if (firstWait)
                        {
                            m_isLongWaiting = speedSign*(displayedTime - ct) > MAX_FRAME_DURATION_MS*1000;
                            if (m_jumpTime != DATETIME_NOW)
                                m_isLongWaiting &= speedSign*(displayedTime - m_jumpTime)  > MAX_FRAME_DURATION_MS*1000;
                            if (m_isLongWaiting && !doDelayForAudio && m_audioDisplay->isPlaying())
                            {
                                isAudioSuspended = true;
                                m_audioDisplay->suspend();
                            }

                            /*
                            qDebug() << "displayedTime=" << QDateTime::fromMSecsSinceEpoch(displayedTime/1000).toString("hh:mm:ss.zzz")
                                     << " currentTime=" << QDateTime::fromMSecsSinceEpoch(ct/1000).toString("hh:mm:ss.zzz")
                                     << " wait=" << (displayedTime - ct)/1000.0;
                            */

                            firstWait = false;
                        }
                        QnSleep::msleep(1);
                    }
                    else
                    {
                        break;
                    }
                }
                if (isAudioSuspended)
                    m_audioDisplay->resume();
                m_isLongWaiting = false;
                /*
                if (sleepTimer.elapsed() > 0)
                    realSleepTime = sleepTimer.elapsed()*1000;
                else
                    realSleepTime = sign * (displayedTime - m_extTimeSrc->expectedTime());
                */
                if (m_extTimeSrc)
                    realSleepTime = speedSign * (displayedTime - m_extTimeSrc->expectedTime());
                else
                    realSleepTime = AV_NOPTS_VALUE;
                //qDebug() << "sleepTimer=" << sleepTimer.elapsed() << "extSleep=" << realSleepTime;
            }
        }
        else if (!m_display[0]->selfSyncUsed())
        {
            if (m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Displayed)
                realSleepTime = doSmartSleep(needToSleep, speed);
            else
                realSleepTime = m_delay.addQuant(needToSleep);
        }
        //qDebug() << "sleep time: " << needToSleep/1000.0 << "  real:" << realSleepTime/1000.0;
        if (realSleepTime != AV_NOPTS_VALUE && !m_buffering)
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
        QElapsedTimer displayTime;
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

            // Do not draw if computer is very slow and we still wanna sync with audio.
            const bool draw = !ignoreVideo;
            const bool shouldPauseWhenIdle = qnSettings->userIdleTimeoutMSecs() > 0;

            if (draw && !shouldPauseWhenIdle)
                updateActivity();

            if (!(vd->flags & QnAbstractMediaData::MediaFlags_Ignore))
            {
                NX_MUTEX_LOCKER lock( &m_timeMutex );
                m_lastDecodedTime = vd->timestamp;
            }

            m_lastFrameDisplayed = m_display[channel]->display(vd, draw, scaleFactor);
            if (m_isStillImage && m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Skipped)
                m_eofSignalSent = true;
        }

        if (m_lastFrameDisplayed == QnVideoStreamDisplay::Status_Displayed)
        {
            if (speed < 0)
                m_nextReverseTime[channel] = m_display[channel]->nextReverseTime();

            m_timeMutex.lock();
            if (m_buffering && !m_archiveReader->isJumpProcessing() && !m_afterJump &&
                m_skippingFramesTime == qint64(AV_NOPTS_VALUE))
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

    }
    return doProcessPacket;
}

bool QnCamDisplay::isAudioBuffering() const
{
    return m_playAudio && m_buffering;
}

bool QnCamDisplay::doDelayForAudio(QnConstCompressedAudioDataPtr ad, float speed)
{
    if (ad->flags & QnAbstractMediaData::MediaFlags_LIVE)
        return true; // no delay for audio live is required
    if (isAudioBuffering())
        return true;

    if (m_extTimeSrc && m_extTimeSrc->isEnabled())
    {
        qint64 displayedTime = getCurrentTime();
        if (speed != 0  && displayedTime != AV_NOPTS_VALUE)
        {
            bool firstWait = true;
            QElapsedTimer sleepTimer;
            sleepTimer.start();
            while (!m_afterJump && !isAudioBuffering() && !needToStop() && sign(m_speed) == sign(speed) && useSync(ad) && !m_singleShotMode)
            {
                qint64 ct = m_extTimeSrc->getCurrentTime();
                qint64 newDisplayedTime = getCurrentTime();
                if (newDisplayedTime != displayedTime)
                {
                    displayedTime = newDisplayedTime;
                    firstWait = true;
                }

                if (ct != DATETIME_NOW && (displayedTime - ct) > 0)
                {
                    if (firstWait)
                    {
                        m_isLongWaiting = (displayedTime - ct) > MAX_FRAME_DURATION_MS*1000;
                        if (m_jumpTime != DATETIME_NOW)
                            m_isLongWaiting &= (displayedTime - m_jumpTime)  > MAX_FRAME_DURATION_MS*1000;
                        firstWait = false;
                        if (m_isLongWaiting)
                            m_audioDisplay->suspend();

                    }
                    QnSleep::msleep(1);
                }
                else
                {
                    break;
                }
            }
            if (m_isLongWaiting)
                m_audioDisplay->resume();
            m_isLongWaiting = false;
        }
    }
    else
    {
        qint32 needToSleep = qMin(MAX_FRAME_DURATION_MS * 1000, ad->timestamp - m_previousVideoTime);
        needToSleep *= 1.0/qAbs(speed);
        doSmartSleep(needToSleep, speed);
        m_previousVideoTime = ad->timestamp;
    }
    m_isLongWaiting = false;

    return true;
}

template <class T>
void QnCamDisplay::markIgnoreBefore(const T& queue, qint64 time)
{
    for (int i = 0; i < queue.size(); ++i)
    {
        QnAbstractMediaDataPtr media = std::dynamic_pointer_cast<QnAbstractMediaData>(queue.at(i));
        if (media)
        {
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

    {
        auto randomAccess = m_dataQueue.lock();
        markIgnoreBefore(randomAccess, time);
    }
    NX_MUTEX_LOCKER lock( &m_timeMutex );
    m_singleShotQuantProcessed = false;
    m_buffering = getBufferingMask();
    m_skippingFramesTime = time;

    if (m_speed >= 0)
        blockTimeValue(qMax(time, getCurrentTime()));
    else
        blockTimeValue(qMin(time, getCurrentTime()));

    m_emptyPacketCounter = 0;
    if (m_extTimeSrc && m_eofSignalSent)
        notifyExternalTimeSrcAboutEof(false);
}

void QnCamDisplay::blockTimeValue(qint64 time)
{
    if (!m_doNotChangeDisplayTime)
    {
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        {
            m_nextReverseTime[i] = AV_NOPTS_VALUE;
            m_display[i]->blockTimeValue(time);
        }
        NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
        m_audioDisplay->blockTimeValue(time);
    }
}

void QnCamDisplay::blockTimeValueSafe(qint64 time)
{
    if (!m_doNotChangeDisplayTime)
    {
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        {
            m_nextReverseTime[i] = AV_NOPTS_VALUE;
            m_display[i]->blockTimeValueSafe(time);
        }
        NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
        m_audioDisplay->blockTimeValue(time);
    }
}

void QnCamDisplay::waitForFramesDisplayed()
{
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        m_display[i]->waitForFramesDisplayed();
}

void QnCamDisplay::onBeforeJump(qint64 time)
{
    m_lastMediaEventTimeout.invalidate();
    if (m_extTimeSrc)
        m_extTimeSrc->onBufferingStarted(this, m_doNotChangeDisplayTime ? getDisplayedTime() : time);

    NX_MUTEX_LOCKER lock( &m_timeMutex );
    onRealTimeStreamHint(time == DATETIME_NOW && m_speed >= 0);

    m_lastDecodedTime = AV_NOPTS_VALUE;
    blockTimeValueSafe(time);
    m_doNotChangeDisplayTime = false;

    m_emptyPacketCounter = 0;
    if (m_extTimeSrc && m_eofSignalSent && time != DATETIME_NOW)
    {
        // Function `m_extTimeSrc->onEofReached` is used for EOF logic.
        // This call is required to prevent unexpected EOF signal after async seek() call.
        // But variable `m_eofSignalSent` is used to display state in UI only.
        // So we do not update this variable unless 1-st frame is received. Otherwise client could
        // get change `true` -> `false` -> `true` state if seek to the same position.
        m_extTimeSrc->onEofReached(this, false);
    }
    clearUnprocessedData();
    {
        NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
        m_audioDisplay->clearAudioBuffer();
    }

    m_buffering = getBufferingMask();

    m_processedPackets = 0;
    m_gotKeyDataInfo.clear();
}

void QnCamDisplay::onJumpOccurred(qint64 time)
{
    //if (m_extTimeSrc)
    //    m_extTimeSrc->onBufferingStarted(this, time);

    NX_MUTEX_LOCKER lock( &m_timeMutex );
    m_afterJump = true;
    m_bofReceived = false;
    m_buffering = getBufferingMask();
    m_lastDecodedTime = AV_NOPTS_VALUE;
    m_firstAfterJumpTime = AV_NOPTS_VALUE;

    m_singleShotQuantProcessed = false;
    m_jumpTime = time;

    m_processedPackets = 0;
    m_delayedFrameCount = 0;
}

void QnCamDisplay::afterJump(QnAbstractMediaDataPtr media)
{
    QnCompressedVideoDataPtr vd = std::dynamic_pointer_cast<QnCompressedVideoData>(media);

    clearVideoQueue();
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        m_display[i]->afterJump();

    NX_MUTEX_LOCKER lock( &m_timeMutex );
    m_lastAudioPacketTime = media->timestamp;
    m_lastVideoPacketTime = media->timestamp;
    m_previousVideoTime = media->timestamp;
    m_lastFrameDisplayed = QnVideoStreamDisplay::Status_Skipped;
    //m_previousVideoDisplayedTime = 0;
    m_totalFrames = 0;
    m_iFrames = 0;
    m_lastSleepInterval = 0;
    if (!m_afterJump && m_skippingFramesTime == qint64(AV_NOPTS_VALUE)) // if not more (not handled yet) jumps expected
    {
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        {
            if (media && !(media->flags & QnAbstractMediaData::MediaFlags_Ignore))
                m_display[i]->blockTimeValue(media->timestamp);

            m_nextReverseTime[i] = qint64(AV_NOPTS_VALUE);
            m_display[i]->unblockTimeValue();
        }
    }
    {
        NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
        m_audioDisplay->clearAudioBuffer();
    }
    m_firstAfterJumpTime = qint64(AV_NOPTS_VALUE);
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

void QnCamDisplay::onPrevFrameOccurred()
{
    if (getDisplayedTime() != DATETIME_NOW)
        m_doNotChangeDisplayTime = true; // do not move display time to jump position because jump pos given approximatly
    NX_MUTEX_LOCKER lock( &m_audioChangeMutex );
    m_audioDisplay->clearDeviceBuffer();
}

void QnCamDisplay::onNextFrameOccurred()
{
    m_singleShotQuantProcessed = false;
    NX_MUTEX_LOCKER lock( &m_audioChangeMutex );
    m_audioDisplay->clearDeviceBuffer();
}

void QnCamDisplay::setSingleShotMode(bool single)
{
    m_singleShotMode = single;
    if (m_singleShotMode)
    {
        m_isRealTimeSource = false;
        emit liveMode(false);

        setMTDecoding(false);

        NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
        m_audioDisplay->suspend();
    }
    else
    {
        NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
        // AudioPackets are filtered out if `m_playAudio` is false, so we omit additional checks.
        m_audioDisplay->resume();
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
    NX_MUTEX_LOCKER lock( &m_timeMutex );
    if (qAbs(speed-m_speed) > FPS_EPS)
    {
        if ((speed >= 0 && m_prevSpeed < 0) || (speed < 0 && m_prevSpeed >= 0))
        {
            m_afterJumpTimer.restart();
            m_executingChangeSpeed = true; // do not show "No data" while display preparing for new speed.
            qint64 time = getExternalTime();
            if (time != DATETIME_NOW)
                blockTimeValue(time);
        }
        if (speed < 0 && m_speed >= 0)
        {
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
    NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
    m_audioDisplay->unblockTimeValue();
}

void QnCamDisplay::notifyExternalTimeSrcAboutEof(bool isEof)
{
    if (m_extTimeSrc)
        m_extTimeSrc->onEofReached(this, isEof);
    m_eofSignalSent = isEof;
}

void QnCamDisplay::processNewSpeed(float speed)
{
    if (qAbs(speed - 1.0) > FPS_EPS && qAbs(speed) > FPS_EPS)
    {
        NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
        m_audioDisplay->clearAudioBuffer();
    }

    if (!m_forceMtDecoding)
    {
        bool mtDecoding = m_useMtDecoding; //< Current recommended value.
        if (qAbs(speed) > 1.0 + FPS_EPS || speed < 0)
            mtDecoding = true; //< Force MT decoding on for fast speed and negative speed.
        else if (speed == 0)
            mtDecoding = false; //< Force MT decoding off for previous/next frame.
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; i++)
            m_display[i]->setMTDecoding(mtDecoding);
    }

    if (speed < 0 && m_prevSpeed >= 0)
        m_buffering = getBufferingMask(); // decode first gop is required some time

    if (m_prevSpeed == 0 && m_extTimeSrc && m_eofSignalSent)
        notifyExternalTimeSrcAboutEof(false);

    // Speed sign was previously changed. Need unblock blocked resources
    if (m_executingChangeSpeed)
    {
        //m_dataQueue.clear();
        clearVideoQueue();
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
            m_display[i]->afterJump();
        NX_MUTEX_LOCKER lock( &m_timeMutex );
        m_lastDecodedTime = AV_NOPTS_VALUE;
        for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
            m_nextReverseTime[i] = AV_NOPTS_VALUE;
        unblockTimeValue();
    }
    if (qAbs(speed) > 1.0) {
        m_storedMaxQueueSize = m_dataQueue.maxSize();
        m_dataQueue.setMaxSize(maxDataQueueSize(QueueSizeType::normalStream));
        m_delay.setMaxOverdraft(-1);
    }
    else
    {
        m_dataQueue.setMaxSize(m_storedMaxQueueSize);
        m_delay.setMaxOverdraft(DEFAULT_DELAY_OVERDRAFT);
        m_delay.afterdelay();
    }
    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_display[i])
            m_display[i]->setSpeed(speed);
    }
    setLightCPUMode(QnAbstractVideoDecoder::DecodeMode_Full);
    m_executingChangeSpeed = false;
}

bool QnCamDisplay::useSync(QnConstAbstractMediaDataPtr md)
{
    auto camera = qSharedPointerDynamicCast<QnVirtualCameraResource>(m_resource);
    return
        m_extTimeSrc
        && m_extTimeSrc->isEnabled()
        && !(md->flags & (QnAbstractMediaData::MediaFlags_LIVE | QnAbstractMediaData::MediaFlags_PlayUnsync))
        && !(camera
            && camera->hasCameraCapabilities(nx::vms::api::DeviceCapability::synchronousChannel));
}

void QnCamDisplay::putData(const QnAbstractDataPacketPtr& data)
{
    // Put analytics/motion packets directly to their conumers to save space in the dataQueue.
    if (const auto& metadata = std::dynamic_pointer_cast<QnAbstractCompressedMetadata>(data))
    {
        if (metadata->metadataType == MetadataType::ObjectDetection
            || metadata->metadataType == MetadataType::Motion)
        {
            pushMetadataToConsumers(metadata);
            return;
        }
    }

    QnCompressedVideoDataPtr video = std::dynamic_pointer_cast<QnCompressedVideoData>(data);
    if (video)
    {
        m_lastQueuedVideoTime = video->timestamp;

        if (video->flags.testFlag(QnAbstractMediaData::MediaFlags_LIVE)
            && m_dataQueue.size() > 0
            && video->timestamp - m_lastVideoPacketTime > m_liveBufferSizeMkSec)
        {
            m_delay.breakSleep();
        }
    }

    QnAbstractDataConsumer::putData(data);
    if (video && m_dataQueue.size() < 2)
        hurryUpCkeckForCamera2(video); // check if slow network

    NX_VERBOSE(this, "Media queue size for resource %1 is %2 packets",
        m_resource, m_dataQueue.size());
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
    NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
    qint64 aTime = m_audioDisplay->startBufferingTime();
    if (aTime == qint64(AV_NOPTS_VALUE))
        return false;

    return vTime > aTime;
    //return m_audioDisplay->isBuffering() && !flushCurrentBuffer;
}

void QnCamDisplay::processSkippingFramesTime()
{
    NX_MUTEX_LOCKER lock( &m_timeMutex );
    if (m_skippingFramesTime != qint64(AV_NOPTS_VALUE))
    {
        for (int i = 0; i < CL_MAX_CHANNELS; ++i)
            markIgnoreBefore(m_videoQueue[i], m_skippingFramesTime);
        m_skippingFramesTime = qint64(AV_NOPTS_VALUE);
    }
}

void QnCamDisplay::moveTimestampTo(qint64 timestampUs)
{
    //performing before locking m_timeMutex. Otherwise we could get dead-lock between this thread and a setSpeed, called from main thread.
    //E.g. overrideTimestampOfNextFrameToRender waits for frames rendered, setSpeed (main thread) waits for m_timeMutex
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        m_display[i]->flushFramesToRenderer();

    m_timeMutex.lock();
    m_lastDecodedTime = AV_NOPTS_VALUE;
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i) {
        if (m_display[i])
            m_display[i]->overrideTimestampOfNextFrameToRender(timestampUs);
        m_nextReverseTime[i] = AV_NOPTS_VALUE;
    }

    if (m_buffering && !m_archiveReader->isJumpProcessing())
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

bool QnCamDisplay::isNvrFillerPacket(qint64 timestampUs) const
{
    return timestampUs != 0 && timestampUs != DATETIME_NOW;
}

void QnCamDisplay::processFillerPacket(
    qint64 timestampUs,
    QnAbstractStreamDataProvider* dataProvider,
    QnAbstractMediaData::MediaFlags flags)
{
    // Do 'jump to live' either for real cameras or '.nov' file items
    const bool hasUtcTime = m_resource
        && m_resource->toResource()->hasFlags(Qn::ResourceFlag::utc);

    m_emptyPacketCounter++;
    const bool needFlushAudio = m_playAudio && m_audioDisplay && m_audioDisplay->msInBuffer() > 0;
    if (needFlushAudio)
        return; //< Play the rest of audio buffer before jump to live.

    // empty data signal about EOF, or read/network error. So, check counter before EOF signaling
    //bool playUnsync = (emptyData->flags & QnAbstractMediaData::MediaFlags_PlayUnsync);
    bool isFillerPacket = isNvrFillerPacket(timestampUs);

    if (m_lastMediaEventTimeout.isValid())
    {
        if (m_lastMediaEventTimeout.hasExpired(kMediaMessageDelay) && !m_eofSignalSent)
        {
            moveTimestampTo(timestampUs);
            notifyExternalTimeSrcAboutEof(true);
        }
    }
    else if (m_emptyPacketCounter >= 3 || isFillerPacket)
    {
        bool isLive = flags & QnAbstractMediaData::MediaFlags_LIVE;
        if (m_extTimeSrc &&
            !isLive &&
            hasUtcTime &&
            !m_eofSignalSent &&
            !isFillerPacket)
        {
            notifyExternalTimeSrcAboutEof(true);
        }

        moveTimestampTo(timestampUs);
    }
    else {
        QnArchiveStreamReader* archive = dynamic_cast<QnArchiveStreamReader*>(dataProvider);
        if (archive && archive->isSingleShotMode())
            archive->needMoreData();
    }
}

void QnCamDisplay::processMetadata(const QnAbstractCompressedMetadataPtr& metadata)
{
    if (metadata->metadataType == MetadataType::MediaStreamEvent)
    {
        const QByteArray data = QByteArray::fromRawData(metadata->data(), (int)metadata->dataSize());

        {
            NX_MUTEX_LOCKER locker(&m_lastMediaEventMutex);
            nx::vms::common::deserialize(data, &m_lastMediaEvent);
        }

        if (m_lastMediaEvent.code == nx::vms::common::MediaStreamEvent::cannotDecryptMedia)
        {
            // For 'cannotDecryptMedia' event just show 'cannotDecryptMedia' or 'no data' overlay.
            NX_MUTEX_LOCKER lock(&m_timeMutex);
            int speedSign = m_speed >= 0 ? 1 : -1;
            m_isLongWaiting = useSync(metadata) && speedSign * (metadata->timestamp
                - m_extTimeSrc->getCurrentTime()) > MAX_FRAME_DURATION_MS * 1000;
            moveTimestampTo(m_speed >= 0 ? std::max(metadata->timestamp, getCurrentTime())
                : std::min(metadata->timestamp, getCurrentTime()));
        }
        else
        {
            // For other events jump to live after some delay.

            if (!m_lastMediaEventTimeout.isValid())
                m_lastMediaEventTimeout.restart();
            processFillerPacket(
                m_speed >= 0 ? DATETIME_NOW : 0,
                metadata->dataProvider,
                QnAbstractMediaData::MediaFlags_None);
        }
    }

    pushMetadataToConsumers(metadata);
}

void QnCamDisplay::pushMetadataToConsumers(const QnAbstractCompressedMetadataPtr & metadata)
{
    NX_MUTEX_LOCKER lock(&m_metadataConsumersHashMutex);
    const auto consumers = m_metadataConsumerByType;
    lock.unlock();

    int consumersCount = 0;
    for (const auto& value: consumers.values(metadata->metadataType))
    {
        if (auto consumer = value.lock())
        {
            ++consumersCount;
            consumer->processMetadata(metadata);
        }
    }

    NX_VERBOSE(this, "Metadata [%1] processed by %2 consumers",
        (int) metadata->metadataType, consumersCount);
}

void QnCamDisplay::endOfRun()
{
    base_type::endOfRun();
    for (auto& display: m_display)
    {
        if (display)
            display->endOfRun();
    }
}

bool QnCamDisplay::processData(const QnAbstractDataPacketPtr& data)
{
    QnAbstractMediaDataPtr media = std::dynamic_pointer_cast<QnAbstractMediaData>(data);
    if (!media)
        return true;

    if (const auto& metadata = std::dynamic_pointer_cast<QnAbstractCompressedMetadata>(data))
        processMetadata(metadata);

    QnCompressedVideoDataPtr vd = std::dynamic_pointer_cast<QnCompressedVideoData>(data);

    if (vd && vd->channelNumber <= CL_MAX_CHANNELS)
    {
        NX_MUTEX_LOCKER lock(&m_timeMutex);
        if (vd->flags.testFlag(QnAbstractMediaData::MediaFlags_AVKey))
        {
            if (m_gotKeyDataInfo.size() <= vd->channelNumber)
                m_gotKeyDataInfo.resize(vd->channelNumber + 1);
            m_gotKeyDataInfo[vd->channelNumber] = true;
        }
        else
        {
            if (m_gotKeyDataInfo.size() <= vd->channelNumber ||
                !m_gotKeyDataInfo[vd->channelNumber])
            {
                NX_DEBUG(this, "Ignore video data with timestamp %1. Waiting for I-frame", vd->timestamp);
                return true; //< Wait for key frame.
            }
        }
    }

    QnCompressedAudioDataPtr ad = std::dynamic_pointer_cast<QnCompressedAudioData>(data);

    m_processedPackets++;

    m_hasVideo = m_resource->hasVideo(media->dataProvider);

    if (media->dataType == QnAbstractMediaData::VIDEO ||
        media->dataType == QnAbstractMediaData::AUDIO)
    {
        NX_MUTEX_LOCKER locker(&m_lastMediaEventMutex);
        if (m_lastMediaEvent.code != nx::vms::common::MediaStreamEvent::noEvent && m_extTimeSrc)
        {
            qint64 currentTime = m_extTimeSrc->getCurrentTime();
            if (currentTime != AV_NOPTS_VALUE)
                m_archiveReader->jumpTo(currentTime, currentTime);
        }
        m_lastMediaEvent = nx::vms::common::MediaStreamEventPacket();
    }

    if (media->dataType != QnAbstractMediaData::EMPTY_DATA)
    {
        bool mediaIsLive = media->flags & QnAbstractMediaData::MediaFlags_LIVE;

        m_timeMutex.lock();
        if (mediaIsLive != m_isRealTimeSource && !m_buffering)
            onRealTimeStreamHint(mediaIsLive && !m_singleShotMode && m_speed > 0);
        m_timeMutex.unlock();
    }

    float speed = m_speed;
    if (media->dataType != QnAbstractMediaData::GENERIC_METADATA)
    {
        bool speedIsNegative = speed < 0;
        bool dataIsNegative = media->flags & QnAbstractMediaData::MediaFlags_Reverse;
        if (speedIsNegative != dataIsNegative)
            return true; // skip data
    }

    if (m_prevSpeed != speed || m_executingChangeSpeed)
    {
        processNewSpeed(speed);
        m_prevSpeed = speed;
    }

    processSkippingFramesTime();

    if (vd)
        m_ignoringVideo = vd->flags & QnAbstractMediaData::MediaFlags_Ignore;

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
        if (m_jumpTime != DATETIME_NOW && ad && ad->timestamp < m_jumpTime - m_audioBufferSize/2*1000)
            return true; // skip packet
        // clear everything we can
        m_bofReceived = false;
        {
            NX_MUTEX_LOCKER lock( &m_timeMutex );
            if (!m_archiveReader->isJumpProcessing())
                m_afterJump = false;
        }
        afterJump(media);
    }
    else if (media->flags & QnAbstractMediaData::MediaFlags_NewServer)
    {
        afterJump(media);
        if (m_extTimeSrc)
            m_extTimeSrc->reinitTime(AV_NOPTS_VALUE);
    }

    if (ad)
    {
        if (speed < 0)
        {
            m_lastAudioPacketTime = ad->timestamp;
            if (!m_hasVideo)
            {
                if (m_buffering) {
                    m_buffering = 0;
                    if (m_extTimeSrc)
                        m_extTimeSrc->onBufferingFinished(this);
                    m_lastAudioPacketTime = AV_NOPTS_VALUE;
                    unblockTimeValue();
                }
                return false; // block data stream
            }
            return true; // ignore audio packet to prevent after jump detection
        }
    }

    QnEmptyMediaDataPtr emptyData = std::dynamic_pointer_cast<QnEmptyMediaData>(data);

    bool flushCurrentBuffer = false;
    int expectedBufferSize = m_isRealTimeSource ? REALTIME_AUDIO_BUFFER_SIZE : DEFAULT_AUDIO_BUFF_SIZE;
    AudioFormat currentAudioFormat;
    bool audioParamsChanged = false;
    if (ad)
    {
        currentAudioFormat = { nx::audio::formatFromMediaContext(ad->context),
            ad->context->getBitsPerCodedSample() };
        audioParamsChanged = m_playingFormat != currentAudioFormat
            || m_audioDisplay->getAudioBufferSize() != expectedBufferSize;
    }
    if (((media->flags & QnAbstractMediaData::MediaFlags_AfterEOF) || audioParamsChanged) &&
        m_videoQueue[0].size() > 0)
    {
        // skip data (play current buffer
        flushCurrentBuffer = true;
    }
    else if (emptyData && m_videoQueue[0].size() > 0)
    {
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
        bool isVideoCamera = qSharedPointerDynamicCast<QnVirtualCameraResource>(m_resource) != 0;
        if (!emptyData->flags.testFlag(QnAbstractMediaData::MediaFlags_AfterEOF) && isVideoCamera
            && !isNvrFillerPacket(emptyData->timestamp))
        {
            // Local EOF packet could be created on TCP stream reconnect.
            // Ignore such packets for video cameras.
            // Also it could be created when position is out of playback mask,
            // in that case MediaFlags_AfterEOF is set and we should process the packet normally.
            return true;
        }
        processFillerPacket(emptyData->timestamp, emptyData->dataProvider, emptyData->flags);
        return true;
    }
    else
    {
        if (m_extTimeSrc && m_eofSignalSent)
            notifyExternalTimeSrcAboutEof(false);
        m_emptyPacketCounter = 0;
    }

    if (ad && !flushCurrentBuffer)
    {
        if (audioParamsChanged)
        {
            int audioPrebufferSize = m_isRealTimeSource ? REALTIME_AUDIO_PREBUFFER : DEFAULT_AUDIO_BUFF_SIZE/2;
            NX_MUTEX_LOCKER lock( &m_audioChangeMutex );
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

        if (!m_hasVideo)
        {
            if (m_singleShotMode && m_singleShotQuantProcessed)
                return false;

            if (!m_playAudio || m_audioDisplay->isPlaying())
            {
                if (m_buffering) {
                    m_buffering = 0;
                    unblockTimeValue();
                    if (m_extTimeSrc)
                        m_extTimeSrc->onBufferingFinished(this);
                }
            }

            doDelayForAudio(ad, speed);
        }

        // we synch video to the audio; so just put audio in player with out thinking
        if (m_playAudio && qAbs(speed-1.0) < FPS_EPS)
        {
            NX_VERBOSE(this, "Sync video to audio for resource %1. "
                "Current audio buffer size %2, maxSize = %3, isRTS=%4",
                m_resource, m_audioDisplay->msInBuffer(), m_audioBufferSize, m_isRealTimeSource);
            if (m_audioDisplay->msInBuffer() > m_audioBufferSize)
            {
                bool useSync = m_extTimeSrc && m_extTimeSrc->isEnabled() && m_hasVideo;
                if (m_isRealTimeSource || useSync)
                {
                    NX_VERBOSE(this, "Skip audio data for resource %1. Current audio buffer size %2 "
                        "is too large, maxSize = %3",
                        m_resource, m_audioDisplay->msInBuffer(), m_audioBufferSize);
                    return true; // skip data
                }
                else
                {
                    QnSleep::msleep(40); // Audio buffer too large. waiting
                }
            }

            if (m_audioDisplay->putData(ad, nextVideoImageTime(0)))
                m_hadAudio = true;
        }
        else if (m_hadAudio || !m_hasVideo)
        {
            m_audioDisplay->enqueueData(ad, nextVideoImageTime(0));
        }
        if (!m_hasVideo)
            m_singleShotQuantProcessed = true;
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
                    if (!(vd->flags & QnAbstractMediaData::MediaFlags_ReverseBlockStart) && vd->timestamp - m_lastVideoPacketTime < -MIN_VIDEO_DETECT_JUMP_INTERVAL*3)
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

            if(m_display[channel] != nullptr)
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
                if (!m_useMtDecoding && !m_isRealTimeSource)
                    setMTDecoding(true);

                bool ignoreVideo = vd->flags & QnAbstractMediaData::MediaFlags_Ignore;
                if (!ignoreVideo) {
                    NX_MUTEX_LOCKER lock( &m_timeMutex );
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
}

void QnCamDisplay::setLightCPUMode(QnAbstractVideoDecoder::DecodeMode val)
{
    if (val == m_lightCpuMode)
        return;

    NX_WARNING(this, lit("set CPUMode=%1").arg(val));

    for (int i = 0; i < CL_MAX_CHANNELS; ++i)
    {
        if (m_display[i])
            m_display[i]->setLightCPUMode(val);
    }
    m_lightCpuMode = val;
}

void QnCamDisplay::playAudio(bool play)
{
    if (m_resource->toResourcePtr()->hasFlags(Qn::virtual_camera))
    {
        m_shouldPlayAudio = play;
        QnSecurityCamResourcePtr camera =
            m_resource->toResourcePtr().dynamicCast<QnSecurityCamResource>();
        if (camera && !camera->isAudioEnabled())
            play = false; //< Ignore audio for virtual cameras if it is disabled in UI.
    }

    if (m_playAudio == play)
        return;

    if (m_singleShotMode && play)
        return; // ignore audio playing if camDisplay is paused

    m_needChangePriority = true;
    m_playAudio = play;
    {
        NX_MUTEX_LOCKER lock( &m_audioChangeMutex );
        if (!m_playAudio)
            m_audioDisplay->clearDeviceBuffer();
        else
            m_audioDisplay->resume();
    }

    if (!m_forceMtDecoding)
    {
        if (m_isRealTimeSource)
            setMTDecoding(play && m_useMTRealTimeDecode);
        else
            setMTDecoding(play);
    }
}

// ==========================================================================

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
    //return m_videoQueue->last()->timestamp - m_videoQueue->first()->timestamp >= MAX_FRAME_DURATION_MS*1000ll;
    return m_videoQueueDuration > m_audioDisplay->getAudioBufferSize() * 2 * 1000;
}

QnCompressedVideoDataPtr QnCamDisplay::dequeueVideo(int channel)
{
    if (m_videoQueue[channel].size() > 1)
    {
        qint64 timeDiff = m_videoQueue[channel].at(1)->timestamp - m_videoQueue[channel].front()->timestamp;
        if (timeDiff <= MAX_FRAME_DURATION_MS*1000ll) // ignore data holes
            m_videoQueueDuration -= timeDiff;
    }
    return m_videoQueue[channel].dequeue();
}

void QnCamDisplay::enqueueVideo(QnCompressedVideoDataPtr vd)
{
    if (!m_videoQueue[vd->channelNumber].isEmpty())
    {
        qint64 timeDiff = vd->timestamp - m_videoQueue[vd->channelNumber].last()->timestamp;
        if (timeDiff <= MAX_FRAME_DURATION_MS*1000ll) // ignore data holes
            m_videoQueueDuration += timeDiff;
    }
    m_videoQueue[vd->channelNumber].enqueue(vd);
    if (m_videoQueue[vd->channelNumber].size() > 60 * 6) // I assume we are not gonna buffer
    {
        NX_WARNING(this, "Video buffer overflow!");
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
        //m_realTimeHurryUp = true;
    }
}

void QnCamDisplay::onRealTimeStreamHint(bool value)
{
    if (value == m_isRealTimeSource)
        return;
    m_isRealTimeSource = value;
    if (m_isRealTimeSource)
    {
        m_liveBufferSizeMkSec = initialLiveBufferMkSecs();
        m_liveMaxLenReached = false;
        m_realTimeHurryUp = false;
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
    m_dataQueue.setMaxSize(maxDataQueueSize(QueueSizeType::slowStream));
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
    qint64 rez = AV_NOPTS_VALUE;
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
    {
        qint64 val = m_display[i]->getTimestampOfNextFrameToRender();
        if (val == AV_NOPTS_VALUE)
            continue;

        if (rez == AV_NOPTS_VALUE)
            rez = val;
        else
            rez = qMin(rez, val);
    }
    return rez;
}

qint64 QnCamDisplay::getCurrentTime() const
{
    if (!m_hasVideo) {
        NX_MUTEX_LOCKER lock(&m_audioChangeMutex);
        if (m_speed < 0)
            return AV_NOPTS_VALUE;
        else
            return m_audioDisplay->getCurrentTime();
    }
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
        if (m_nextReverseTime[i] != AV_NOPTS_VALUE && m_nextReverseTime[i] < rez)
            rez = m_nextReverseTime[i];
    }
    return rez;
}

qint64 QnCamDisplay::getNextTime() const
{
    if (!m_hasVideo) {
        if (m_speed < 0)
            return AV_NOPTS_VALUE;
        else
            return m_lastAudioPacketTime; //getCurrentTime();
    }
    if( m_display[0] && m_display[0]->isTimeBlocked() )
        return m_display[0]->getTimestampOfNextFrameToRender();

    qint64 rez = m_speed < 0 ? getMinReverseTime() : m_lastDecodedTime;

    if (!m_display[0])
        return rez;

    qint64 lastTime = m_display[0]->getTimestampOfNextFrameToRender();

    if (rez != AV_NOPTS_VALUE)
        return m_speed < 0 ? qMin(rez, lastTime) : qMax(rez, lastTime);
    else
        return lastTime;

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
    return m_eofSignalSent == true;
}

bool QnCamDisplay::isLongWaiting() const
{
    if (isRealTimeSource())
        return false;

    if (m_archiveReader->isJumpProcessing() || m_executingChangeSpeed || m_buffering)
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

bool QnCamDisplay::isBuffering() const
{
    if (m_buffering == 0)
        return false;
    // for offline resource at LIVE position no any data. Check it
    if (!isRealTimeSource())
        return true; // if archive position then buffering mark should be resetted event for offline resource
    return m_resource->toResource()->isOnline();
}

QnAspectRatio QnCamDisplay::overridenAspectRatio() const
{
    if (m_display[0])
        return m_display[0]->overridenAspectRatio();
    return QnAspectRatio();
}

void QnCamDisplay::setOverridenAspectRatio(QnAspectRatio aspectRatio)
{
    for (int i = 0; i < CL_MAX_CHANNELS && m_display[i]; ++i)
        m_display[i]->setOverridenAspectRatio(aspectRatio);
}

QnMediaResourcePtr QnCamDisplay::resource() const
{
    return m_resource;
}

qint64 QnCamDisplay::initialLiveBufferMkSecs()
{
    return qMin(qnSettings->initialLiveBufferMs() * 1000ll, maximumLiveBufferMkSecs());
}

qint64 QnCamDisplay::maximumLiveBufferMkSecs()
{
    return qnSettings->maximumLiveBufferMs() * 1000ll;
}

nx::vms::common::MediaStreamEventPacket QnCamDisplay::lastMediaEvent() const
{
    NX_MUTEX_LOCKER locker(&m_lastMediaEventMutex);
    return m_lastMediaEvent;
}

QString QnCamDisplay::getName() const
{
    return resource()->toResourcePtr()->getName();
}

bool QnCamDisplay::isRadassSupported() const
{
    const auto camera = m_resource.dynamicCast<QnVirtualCameraResource>();
    return camera && camera->hasDualStreaming();
}

AbstractVideoDisplay::CameraID QnCamDisplay::getCameraID() const
{
    const auto reader = getArchiveReader();
    NX_ASSERT(reader);
    if (!reader)
        return CameraID();

    const auto& camera = reader->getResource().dynamicCast<QnVirtualCameraResource>();
    return camera ? camera->getId() : CameraID();
}

MediaQuality QnCamDisplay::getQuality() const
{
    return getArchiveReader()->getQuality();
}

void QnCamDisplay::setQuality(MediaQuality quality, bool fastSwitch)
{
    NX_ASSERT(m_archiveReader);
    getArchiveReader()->setQuality(quality, fastSwitch);
}

bool QnCamDisplay::isPaused() const
{
    NX_ASSERT(m_archiveReader);
    return getArchiveReader()->isPaused();
}

bool QnCamDisplay::isMediaPaused() const
{
    NX_ASSERT(m_archiveReader);
    return getArchiveReader()->isMediaPaused();
}

int QnCamDisplay::dataQueueSize() const
{
    return QnAbstractDataConsumer::queueSize();
}

int QnCamDisplay::maxDataQueueSize() const
{
    return QnAbstractDataConsumer::maxQueueSize();
}

void QnCamDisplay::setCallbackForStreamChanges(std::function<void()> callback)
{
    m_streamsChangedCallback = callback;
}
