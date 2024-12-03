// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_player.h"

#include <optional>

#include <QtCore/QElapsedTimer>
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMutex>
#include <QtCore/QTimer>
#include <QtMultimedia/QAbstractVideoBuffer>
#include <QtMultimedia/QVideoSink>

#include <core/resource/avi/avi_archive_delegate.h>
#include <core/resource/avi/avi_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/kit/debug.h>
#include <nx/media/ini.h>
#include <nx/media/supported_decoders.h>
#include <nx/media/video_frame.h>
#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/fuzzy.h>
#include <nx/utils/math/math.h>
#include <nx/vms/common/application_context.h>
#include <nx/vms/common/system_context.h>
#include <nx_ec/abstract_ec_connection.h>
#include <utils/common/long_runable_cleanup.h>

#include "audio_output.h"
#include "frame_metadata.h"
#include "media_player_quality_chooser.h"
#include "player_data_consumer.h"
#include "video_decoder_registry.h"

#if NX_MEDIA_QUICK_SYNC_DECODER_SUPPORTED
    #include <nx/media/quick_sync/quick_sync_surface.h>
#endif

static size_t qHash(const MetadataType& value)
{
    return size_t(value);
}

namespace nx {
namespace media {

namespace {

// Max allowed frame duration. If the distance is higher, then the discontinuity is detected.
static constexpr qint64 kMaxFrameDurationMs = 1000 * 5;

static constexpr qint64 kLivePosition = -1;

// Resync playback timer if a video frame is too late.
static constexpr int kMaxDelayForResyncMs = 500;

// Max allowed amount of underflow/overflow issues in Live mode before extending live buffer.
static constexpr int kMaxCounterForWrongLiveBuffer = 2;

// Calculate next time to render later. It used for AV sync in case of audio buffer has hole in the middle.
// At this case current audio playback position may be significant less than video frame PTS.
// Audio and video timings will became similar as soon as audio buffer passes a hole.
static constexpr int kTryLaterIntervalMs = 16;

// Default value for max openGL texture size
static constexpr int kDefaultMaxTextureSize = 2048;

// Player will go to the invalid state if no data is received within this timeout.
static constexpr int kGotDataTimeoutMs = 1000 * 30;

// Periodic tasks timer interval
static constexpr int kPeriodicTasksTimeoutMs = 1000;

static qint64 msecToUsec(qint64 posMs)
{
    return posMs == kLivePosition ? DATETIME_NOW : posMs * 1000ll;
}

static qint64 usecToMsec(qint64 posUsec)
{
    return posUsec == DATETIME_NOW ? kLivePosition : posUsec / 1000ll;
}

using QualityInfo = media_player_quality_chooser::Result;
using QualityInfoList = QList<QualityInfo>;
QList<int> sortOutEqualQualities(QualityInfoList qualities)
{
    QList<int> result;
    if (qualities.isEmpty())
        return result;

    // Removes standard qualities from list which will be sorted.
    const auto newEnd = std::remove_if(qualities.begin(), qualities.end(),
        [](const QualityInfo& value)
        {
            return value.quality < Player::CustomVideoQuality;
        });

    // Adds standard qualities to result.
    for (auto it = newEnd; it != qualities.end(); ++it)
        result.append(it->quality);

    qualities.erase(newEnd, qualities.end());

    std::sort(qualities.begin(), qualities.end(),
        [](const QualityInfo& left, const QualityInfo& right) -> bool
        {
            return left.quality > right.quality;
        });

    for (int i = 0; i < qualities.size() - 1;)
    {
        // Removes neighbor equal values.
        if (qualities[i].frameSize == qualities[i + 1].frameSize)
        {
            qualities.removeAt(i);
            continue; //< Check next equal values.
        }

        ++i;
    }

    for (const auto qualityInfo: qualities)
        result.append(qualityInfo.quality);

    return result;
}

} // namespace

class Player::Private: public QObject
{
    using base_type = QObject;

    Player* const q;

public:
    // Holds QT property value.
    State state = State::Stopped;

    AutoJumpPolicy autoJumpPolicy = AutoJumpPolicy::DefaultJumpPolicy;

    // Holds QT property value.
    MediaStatus mediaStatus = MediaStatus::NoMedia;

    // Either media is on live or archive position. Holds QT property value.
    bool liveMode = true;

    nx::media::StreamEvent lastMediaEvent =
        nx::media::StreamEvent::noEvent;

    // Video aspect ratio
    double aspectRatio = 1.0;

    // Auto reconnect if network error
    bool reconnectOnPlay = false;

    // UTC Playback position at msec. Holds QT property value.
    qint64 positionMs = 0;

    // Playback speed.
    double speed = 1.0;

    // Video surface list to render. Holds QT property value.
    QMap<int, QPointer<QVideoSink>> videoSurfaces;

    // Whether the current resource is a local file.
    bool isLocalFile = false;

    // Resource to play.
    QnResourcePtr resource;

    int maxTextureSize = kDefaultMaxTextureSize;

    // Main AV timer for playback.
    QElapsedTimer ptsTimer;

    // Last video frame PTS.
    std::optional<qint64> lastVideoPtsMs;

    // Timestamp when the PTS timer was started.
    qint64 ptsTimerBaseMs = 0;

    // Decoded video which is awaiting to be rendered.
    VideoFramePtr videoFrameToRender;

    // Separate thread. Performs network IO and gets compressed AV data.
    std::unique_ptr<QnArchiveStreamReader> archiveReader;

    // Separate thread. Decodes compressed AV data.
    std::unique_ptr<PlayerDataConsumer> dataConsumer;

    // Timer for delayed call to presentNextFrame().
    QTimer* const execTimer;

    // Timer for miscs periodic tasks
    QTimer* const miscTimer;

    // Last seek position. UTC time in msec.
    qint64 lastSeekTimeMs = AV_NOPTS_VALUE;

    // Current duration of live buffer in range [kInitialBufferMs .. kMaxLiveBufferMs].
    int liveBufferMs = kInitialBufferMs;

    enum class BufferState
    {
        NoIssue,
        Underflow,
        Overflow,
    };

    // Live buffer state for the last frame.
    BufferState liveBufferState = BufferState::NoIssue;

    // Live buffer underflow counter.
    int underflowCounter = 0;

    // Live buffer overflow counter.
    int overflowCounter = 0;

    // See property comment.
    int videoQuality = VideoQuality::HighVideoQuality;

    bool allowOverlay = true;

    bool allowHardwareAcceleration = true;

    // Video geometry inside the application window.
    QRect videoGeometry;

    // Resolution of the last displayed frame.
    QSize currentResolution;

    // Protects access to videoGeometry.
    mutable QMutex videoGeometryMutex;

    // Interval since last time player got some data
    QElapsedTimer gotDataTimer;

    // Turn on / turn off audio.
    bool isAudioEnabled = true;

    bool isAudioOnlyMode = false;

    RenderContextSynchronizerPtr renderContextSynchronizer;

    using MetadataConsumerList = QList<QWeakPointer<AbstractMetadataConsumer>>;
    QHash<MetadataType, MetadataConsumerList> m_metadataConsumerByType;

    // Hardware decoding has been used for the last presented frame.
    bool isHwAccelerated = false;

    // Human-readable tag for logging and debugging purposes.
    QString tag = "MediaPlayer";

    QnTimePeriodList periods;

public:
    Private(Player* parent);

    void applyVideoQuality();

    void handleMediaEventChanged();

    void at_hurryUp();
    void at_jumpOccurred(int sequence);
    void at_gotVideoFrame();
    void at_gotAudioFrame();
    void at_gotMetadata(const QnAbstractCompressedMetadataPtr& metadata);
    void presentNextFrameDelayed();

    void presentNextFrame();
    qint64 getDelayForNextFrameWithAudioMs(
        const VideoFramePtr& frame,
        const ConstAudioOutputPtr& audioOutput);
    qint64 getDelayForNextFrameWithoutAudioMs(const VideoFramePtr& frame);
    bool createArchiveReader();
    bool initDataProvider();

    void setState(State state);
    void setMediaStatus(MediaStatus status);
    void setLiveMode(bool value);
    void setAspectRatio(double value);
    void setPosition(qint64 value);
    void updateCurrentResolution(const QSize& size);

    void resetLiveBufferState();
    void updateLiveBufferState(BufferState value);

    VideoFramePtr scaleFrame(const VideoFramePtr& videoFrame);

    void doPeriodicTasks();
    void log(const QString& message) const;
    void clearCurrentFrame();
    void configureMetadataForReader();
    void updateAudio();

    /** Push empty video frame to output. */
    void clearVideoOutput();
};

Player::Private::Private(Player* parent):
    base_type(parent),
    q(parent),
    execTimer(new QTimer(this)),
    miscTimer(new QTimer(this)),
    renderContextSynchronizer(VideoDecoderRegistry::instance()->defaultRenderContextSynchronizer())
{
    connect(execTimer, &QTimer::timeout, this, &Private::presentNextFrame);
    execTimer->setSingleShot(true);

    connect(miscTimer, &QTimer::timeout, this, &Private::doPeriodicTasks);
    miscTimer->start(kPeriodicTasksTimeoutMs);
}

void Player::Private::setState(State value)
{
    if (value == state)
        return;

    gotDataTimer.restart();
    state = value;

    emit q->playbackStateChanged();
}

void Player::Private::doPeriodicTasks()
{
    if (state == State::Playing)
    {
        if (dataConsumer)
        {
            auto audio = dataConsumer->audioOutput();
            if (audio && audio->currentBufferSizeUsec() > 0)
            {
                gotDataTimer.restart();
                return;
            }
        }

        if (gotDataTimer.hasExpired(kGotDataTimeoutMs))
        {
            if (mediaStatus != MediaStatus::EndOfMedia)
            {
                log("doPeriodicTasks(): No data, timeout expired => setMediaStatus(NoMedia)");
                setMediaStatus(MediaStatus::NoMedia);
                q->stop();
            }
        }
    }
}

void Player::Private::setMediaStatus(MediaStatus status)
{
    if (mediaStatus == status)
        return;

    mediaStatus = status;
    emit q->mediaStatusChanged();
}

void Player::Private::setLiveMode(bool value)
{
    if (liveMode == value)
        return;

    liveMode = value;
    emit q->liveModeChanged();
}

void Player::Private::setAspectRatio(double value)
{
    if (qFuzzyEquals(aspectRatio, value))
        return;

    aspectRatio = value;
    emit q->aspectRatioChanged();
}

void Player::Private::setPosition(qint64 value)
{
    if (positionMs == value)
        return;

    positionMs = value;
    emit q->positionChanged();
}

void Player::Private::updateCurrentResolution(const QSize& size)
{
    if (currentResolution == size)
        return;

    currentResolution = size;
    emit q->currentResolutionChanged();
}

void Player::Private::at_hurryUp()
{
    if (videoFrameToRender)
    {
        execTimer->stop();
        presentNextFrame();
    }
    else
    {
        QTimer::singleShot(0, this, &Private::at_gotVideoFrame);
    }
}

void Player::Private::at_jumpOccurred(int sequence)
{
    if (!videoFrameToRender)
        return;
    FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);
    if (sequence && sequence != metadata.sequence)
    {
        // Drop deprecate frame
        clearCurrentFrame();
        at_gotVideoFrame();
    }
}

void Player::Private::at_gotAudioFrame()
{
    if (state == State::Stopped)
        return;

    if (!dataConsumer)
        return;

    // TODO lbusygin: move audioOutput to MediaPlayer from dataConsumer?

    auto audioOutput = dataConsumer->audioOutput();
    if (q->isAudioOnlyMode() && audioOutput && dataConsumer->isAudioEnabled())
        setMediaStatus(MediaStatus::Loaded);
}

void Player::Private::at_gotVideoFrame()
{
    if (state == State::Stopped)
        return;

    if (videoFrameToRender)
        return; //< We already have a frame to render. Ignore next frame (will be processed later).

    if (!dataConsumer)
        return;

    videoFrameToRender = dataConsumer->dequeueVideoFrame();
    if (!videoFrameToRender)
        return;

    gotDataTimer.restart();

    FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);

    if (metadata.flags.testFlag(QnAbstractMediaData::MediaFlags_AfterEOF))
    {
        if (metadata.displayHint == DisplayHint::obsolete)
            return;

        videoFrameToRender.reset();
        bool canJumpToLive = q->playbackState() != State::Previewing;
        if (q->autoJumpPolicy() == AutoJumpPolicy::DisableJumpToLive
            || q->autoJumpPolicy() == AutoJumpPolicy::DisableJumpToLiveAndNextChunk)
        {
            canJumpToLive = false;
        }

        if (canJumpToLive)
        {
            log("at_gotVideoFrame(): EOF reached, jumping to LIVE.");
            q->setPosition(kLivePosition);
        }
        else
        {
            log("at_gotVideoFrame(): EOF reached.");
            setMediaStatus(MediaStatus::EndOfMedia);
        }

        return;
    }

    if (state == State::Paused || state == State::Previewing)
    {
        if (metadata.displayHint == DisplayHint::regular)
            return; //< Display regular frames if and only if the player is playing.
    }

    presentNextFrameDelayed();
}

void Player::Private::presentNextFrameDelayed()
{
    if (!videoFrameToRender || !dataConsumer)
        return;

    qint64 delayToRenderMs = 0;
    auto audioOutput = dataConsumer->audioOutput();
    if (audioOutput && dataConsumer->isAudioEnabled())
    {
        if (audioOutput->isBufferUnderflow())
        {
            // If audio buffer is empty we have to display current video frame to unblock data stream
            // and allow audio data to fill the buffer.
            presentNextFrame();
            return;
        }

        delayToRenderMs = getDelayForNextFrameWithAudioMs(videoFrameToRender, audioOutput);

        // If video delay interval is bigger then audio buffer, it'll block audio playing.
        // At this case calculate time again after a delay.
        if (delayToRenderMs > audioOutput->currentBufferSizeUsec() / 1000)
        {
            QTimer::singleShot(kTryLaterIntervalMs, this, &Private::presentNextFrameDelayed); //< calculate next time to render later
            return;
        }
    }
    else
    {
        delayToRenderMs = getDelayForNextFrameWithoutAudioMs(videoFrameToRender);
    }

    if (delayToRenderMs > 0)
    {
        execTimer->start(delayToRenderMs);
    }
    else
    {
        presentNextFrame();
    }
}

VideoFramePtr Player::Private::scaleFrame(const VideoFramePtr& videoFrame)
{
    if (videoFrame->handleType() != QVideoFrame::NoHandle)
        return videoFrame; //< Do not scale any hardware video frames.

    if (videoFrame->width() <= maxTextureSize && videoFrame->height() <= maxTextureSize)
        return videoFrame; //< Scale is not required.

    const QImage image = videoFrame->toImage().scaled(
        maxTextureSize, maxTextureSize, Qt::KeepAspectRatio);

    NX_ASSERT(!image.isNull());

    if (auto scaledFrame = std::make_shared<VideoFrame>(image))
        return scaledFrame;

    return videoFrame;
}

void Player::Private::presentNextFrame()
{
    NX_FPS(PresentNextFrame);

    if (!videoFrameToRender)
        return;

    gotDataTimer.restart();

    if (!videoFrameToRender->isValid())
    {
        videoFrameToRender.reset();
        return; //< Just to update timer on 'filler' frame. Do nothing.
    }

    FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);

    if (videoSurfaces.isEmpty())
        return;

    if (metadata.videoChannel == videoSurfaces.firstKey())
        updateCurrentResolution(videoFrameToRender->size());

    auto videoSurface = videoSurfaces.value(metadata.videoChannel);

    bool isLivePacket = metadata.flags.testFlag(QnAbstractMediaData::MediaFlags_LIVE);
    bool skipFrame = (isLivePacket != liveMode && !isLocalFile)
        || (state != State::Previewing
            && metadata.displayHint == DisplayHint::obsolete);

    if (videoSurface && !skipFrame)
    {
        setMediaStatus(MediaStatus::Loaded);
        isHwAccelerated = metadata.flags.testFlag(QnAbstractMediaData::MediaFlags_HWDecodingUsed);
        videoSurface->setVideoFrame(*scaleFrame(videoFrameToRender));
        if (dataConsumer)
        {
            qint64 timeUs = liveMode ? DATETIME_NOW : videoFrameToRender->startTime() * 1000;
            dataConsumer->setDisplayedTimeUs(timeUs);
        }
        if (metadata.displayHint != DisplayHint::obsolete)
            setPosition(videoFrameToRender->startTime());
        setAspectRatio(videoFrameToRender->width() * metadata.sar / videoFrameToRender->height());
    }
    videoFrameToRender.reset();

    // Calculate next time to render.
    QTimer::singleShot(0, this, &Private::at_gotVideoFrame);
}

void Player::Private::resetLiveBufferState()
{
    overflowCounter = underflowCounter = 0;
    liveBufferState = BufferState::NoIssue;
    liveBufferMs = kInitialBufferMs;
}

void Player::Private::updateLiveBufferState(BufferState value)
{
    if (liveBufferState == value)
        return; //< Do not take into account the same state several times in a row.
    liveBufferState = value;

    if (liveBufferState == BufferState::Overflow)
        overflowCounter++;
    else if (liveBufferState == BufferState::Underflow)
        underflowCounter++;
    else
        return;

    if (underflowCounter + overflowCounter >= kMaxCounterForWrongLiveBuffer)
    {
        // Too much underflow/overflow issues. Extend live buffer.
        liveBufferMs = qMin(liveBufferMs * kBufferGrowStep, kMaxLiveBufferMs);
    }
}

qint64 Player::Private::getDelayForNextFrameWithAudioMs(
    const VideoFramePtr& frame,
    const ConstAudioOutputPtr& audioOutput)
{
    const qint64 currentPosUsec = audioOutput->playbackPositionUsec();
    if (currentPosUsec == AudioOutput::kUnknownPosition)
        return 0; //< Position isn't known yet. Play video without delay.

    qint64 delayToAudioMs = frame->startTime() - currentPosUsec / 1000;
    return delayToAudioMs;
}

qint64 Player::Private::getDelayForNextFrameWithoutAudioMs(const VideoFramePtr& frame)
{
    const qint64 ptsMs = frame->startTime();
    const qint64 ptsDeltaMs = ptsMs - ptsTimerBaseMs;
    FrameMetadata metadata = FrameMetadata::deserialize(frame);

    // Calculate time to present next frame
    qint64 mediaQueueLenMs = usecToMsec(dataConsumer->queueVideoDurationUsec());
    const qint64 elapsedMs = ptsTimer.elapsed();
    const qint64 frameDelayMs = ptsDeltaMs - elapsedMs;
    bool liveBufferUnderflow =
        liveMode && lastVideoPtsMs.has_value() && mediaQueueLenMs == 0 && frameDelayMs < 0;
    bool liveBufferOverflow = liveMode && mediaQueueLenMs > liveBufferMs;

    if (ini().outputFrameDelays)
    {
        if (frameDelayMs < 0)
        {
            NX_PRINT << "ptsMs: " << ptsMs << ", ptsDeltaMs: " << ptsDeltaMs
                << ", frameDelayMs: " << frameDelayMs;
        }
    }

    if (liveMode)
    {
        if (metadata.displayHint != DisplayHint::regular)
        {
            resetLiveBufferState(); //< Reset live statistics because of seek or FCZ frames.
        }
        else
        {
            auto value =
                liveBufferUnderflow ? BufferState::Underflow :
                liveBufferOverflow ? BufferState::Overflow :
                BufferState::NoIssue;
            updateLiveBufferState(value);
        }
    }

    if (!lastVideoPtsMs.has_value() || //< first time
        !qBetween(*lastVideoPtsMs, ptsMs, *lastVideoPtsMs + kMaxFrameDurationMs) || //< pts discontinuity
        metadata.displayHint != DisplayHint::regular || //< jump occurred
        frameDelayMs < -kMaxDelayForResyncMs * speed || //< Resync because the video frame is late for more than threshold.
        liveBufferUnderflow ||
        liveBufferOverflow //< live buffer overflow
        )
    {
        // Reset timer.
        lastVideoPtsMs = ptsMs;
        ptsTimerBaseMs = ptsMs;
        ptsTimer.restart();
        return 0;
    }
    else
    {
        lastVideoPtsMs = ptsMs;
        return (ptsMs - ptsTimerBaseMs) / speed - elapsedMs;
    }
}

void Player::Private::clearVideoOutput()
{
    for (auto [channel, sink]: videoSurfaces.asKeyValueRange())
    {
        if (sink)
            sink->setVideoFrame({});
    }
}


void Player::Private::applyVideoQuality()
{
    if (!archiveReader)
        return;

    auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera || !camera->hasVideo())
        return; //< Setting videoQuality for files is not supported.

    const auto currentVideoDecoders = dataConsumer
        ? dataConsumer->currentVideoDecoders()
        : std::vector<AbstractVideoDecoder*>();

    media_player_quality_chooser::Params input;
    input.transcodingCodec = archiveReader->getTranscodingCodec();
    input.liveMode = liveMode;
    input.positionMs = positionMs;
    input.camera = camera;
    input.allowOverlay = allowOverlay;
    input.allowHardwareAcceleration = allowHardwareAcceleration;
    input.currentDecoders = &currentVideoDecoders;

    const auto& result = media_player_quality_chooser::chooseVideoQuality(videoQuality, input);

    switch (result.quality)
    {
        case UnknownVideoQuality:
            log("applyVideoQuality(): Could not choose quality => setMediaStatus(NoVideoStreams)");
            setMediaStatus(MediaStatus::NoVideoStreams);
            q->stop();
            return;

        case HighVideoQuality:
            archiveReader->setQuality(MEDIA_Quality_High, /*fastSwitch*/ true);
            break;

        case LowVideoQuality:
            archiveReader->setQuality(MEDIA_Quality_Low, /*fastSwitch*/ true);
            break;

        case LowIframesOnlyVideoQuality:
            archiveReader->setQuality(MEDIA_Quality_LowIframesOnly, /*fastSwitch*/ true);
            break;

        default:
            // Use "auto" width for correct aspect ratio, because quality.width() is in logical
            // pixels.
            NX_ASSERT(result.frameSize.isValid());
            archiveReader->setQuality(MEDIA_Quality_CustomResolution, /*fastSwitch*/ true,
                QSize(/*width*/ 0, result.frameSize.height()));
    }
    at_hurryUp(); //< skip waiting for current frame
}

bool Player::Private::createArchiveReader()
{
    if (!resource)
        return false;

    archiveReader.reset(new QnArchiveStreamReader(resource));

    QnAbstractArchiveDelegate* archiveDelegate;
    if (isLocalFile)
    {
        archiveDelegate = new QnAviArchiveDelegate();
    }
    else
    {
        const auto camera = resource.dynamicCast<QnVirtualCameraResource>();
        if (!NX_ASSERT(camera))
            return false;

        const auto systemContext = camera->systemContext();
        if (!NX_ASSERT(systemContext))
            return false;

        auto connection = systemContext->messageBusConnection();
        auto rtspArchiveDelegate = new QnRtspClientArchiveDelegate(
            archiveReader.get(),
            connection ? connection->credentials() : nx::network::http::Credentials(),
            tag);
        rtspArchiveDelegate->setCamera(camera);
        archiveDelegate = rtspArchiveDelegate;
    }

    archiveReader->setArchiveDelegate(archiveDelegate);
    if (!periods.isEmpty())
        archiveReader->setPlaybackMask(periods);

    configureMetadataForReader();

    return true;
}

bool Player::Private::initDataProvider()
{
    if (!createArchiveReader())
    {
        setMediaStatus(MediaStatus::NoMedia);
        return false;
    }

    if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        const bool audioOnlyMode = !camera->hasVideo() && camera->isAudioSupported();
        if (isAudioOnlyMode != audioOnlyMode)
        {
            isAudioOnlyMode = audioOnlyMode;
            emit q->audioOnlyModeChanged();
        }
    }

    applyVideoQuality();

    if (!archiveReader)
        return false;

    dataConsumer.reset(new PlayerDataConsumer(archiveReader, renderContextSynchronizer));
    dataConsumer->setPlaySpeed(speed);
    dataConsumer->setAllowOverlay(allowOverlay);
    dataConsumer->setAllowHardwareAcceleration(allowHardwareAcceleration);
    updateAudio();

    dataConsumer->setVideoGeometryAccessor(
        [guardedThis = QPointer<Private>(this)]()
        {
            QRect r;
            if (guardedThis)
            {
                QMutexLocker lock(&guardedThis->videoGeometryMutex);
                r = guardedThis->videoGeometry;
            }

            if (ini().hwVideoX != -1)
                r.setX(ini().hwVideoX);
            if (ini().hwVideoY != -1)
                r.setY(ini().hwVideoY);
            if (ini().hwVideoWidth != -1)
                r.setWidth(ini().hwVideoWidth);
            if (ini().hwVideoHeight != -1)
                r.setHeight(ini().hwVideoHeight);

            return r;
        });

    archiveReader->addDataProcessor(dataConsumer.get());
    connect(dataConsumer.get(), &PlayerDataConsumer::gotMetadata,
        this, &Private::at_gotMetadata);
    connect(dataConsumer.get(), &PlayerDataConsumer::gotVideoFrame,
        this, &Private::at_gotVideoFrame);
    connect(dataConsumer.get(), &PlayerDataConsumer::gotAudioFrame,
        this, &Private::at_gotAudioFrame);
    connect(dataConsumer.get(), &PlayerDataConsumer::hurryUp,
        this, &Private::at_hurryUp);
    connect(dataConsumer.get(), &PlayerDataConsumer::jumpOccurred,
        this, &Private::at_jumpOccurred);
    connect(dataConsumer.get(), &PlayerDataConsumer::mediaEventChanged,
        this, &Private::handleMediaEventChanged);

    if (!liveMode)
    {
        // Second arg means precise seek.
        archiveReader->jumpTo(msecToUsec(positionMs), msecToUsec(positionMs));
    }

    dataConsumer->start();
    archiveReader->start();

    return true;
}

void Player::Private::handleMediaEventChanged()
{
    if (!dataConsumer)
        return;

    const auto mediaEvent = dataConsumer->mediaEvent().code;
    if (mediaEvent == lastMediaEvent)
        return;
    lastMediaEvent = mediaEvent;
    emit q->tooManyConnectionsErrorChanged();
    emit q->cannotDecryptMediaErrorChanged();
}

void Player::Private::log(const QString& message) const
{
    NX_DEBUG(q, message);
}

void Player::Private::clearCurrentFrame()
{
    execTimer->stop();
    videoFrameToRender.reset();
}

void Player::Private::configureMetadataForReader()
{
    if (!archiveReader)
        return;

    using namespace nx::vms::api;
    bool hasMotionConsumer = !m_metadataConsumerByType.value(MetadataType::Motion).isEmpty();
    bool hasAnalyticsConsumer = !m_metadataConsumerByType.value(MetadataType::ObjectDetection).isEmpty();

    StreamDataFilters filter = StreamDataFilter::media;
    filter.setFlag(StreamDataFilter::motion, hasMotionConsumer);
    filter.setFlag(StreamDataFilter::objects, hasAnalyticsConsumer);
    archiveReader->setStreamDataFilter(filter);
}

void Player::Private::updateAudio()
{
    if (dataConsumer)
    {
        dataConsumer->setAudioEnabled(isAudioEnabled
            && speed <= 2
            && state == State::Playing);
    }
}

void Player::Private::at_gotMetadata(const QnAbstractCompressedMetadataPtr& metadata)
{
    NX_ASSERT(metadata);

    const auto consumers = m_metadataConsumerByType.value(metadata->metadataType);
    for (const auto& value: consumers)
    {
        if (auto consumer = value.lock())
            consumer->processMetadata(metadata);
    }
}

//-------------------------------------------------------------------------------------------------
// Player

Player::Player(QObject *parent):
    QObject(parent),
    d(new Private(this))
{
    NX_DEBUG(this, "Player()");
    ini().reload();
}

Player::~Player()
{
    NX_DEBUG(this, "~Player() BEGIN");
    stop();
    NX_DEBUG(this, "~Player() END");
}

QnResourcePtr Player::resource() const
{
    return d->resource;
}

void Player::setResource(const QnResourcePtr& value)
{
    if (d->resource == value)
        return;

    NX_DEBUG(this, "setResource(\"%1\") BEGIN", value);

    const State currentState = d->state;
    stop();

    setResourceInternal(value);

    if (d->isLocalFile)
        d->setLiveMode(false);

    if (d->resource)
    {
        if (currentState == State::Playing)
            play();
        else if (currentState == State::Paused)
            pause();
    }

    NX_DEBUG(this, "setSource(\"%1\") END", value);
    NX_DEBUG(this, "emit resourceChanged()");
    emit resourceChanged();
}

void Player::setResourceInternal(const QnResourcePtr& resource)
{
    d->resource = resource;
    d->isLocalFile = resource && resource->hasFlags(Qn::local_media);
}

Player::State Player::playbackState() const
{
    return d->state;
}

Player::MediaStatus Player::mediaStatus() const
{
    return d->mediaStatus;
}

QVideoSink* Player::videoSurface(int channel) const
{
    return d->videoSurfaces.value(channel);
}

qint64 Player::position() const
{
    // positionMs is the actual value from a video frame. It could contain a "coarse" timestamp
    // a bit less then the user requested position (inside of the current GOP).
    return qMax(d->lastSeekTimeMs, d->positionMs);
}

Player::AutoJumpPolicy Player::autoJumpPolicy() const
{
    return d->autoJumpPolicy;
}

void Player::setAutoJumpPolicy(AutoJumpPolicy policy)
{
    if (policy == d->autoJumpPolicy)
        return;

    d->autoJumpPolicy = policy;
    emit autoJumpPolicyChanged();
}

void Player::setPosition(qint64 value)
{
    if (value > QDateTime::currentMSecsSinceEpoch())
        value = -1;

    NX_DEBUG(this, "setPosition(%1: %2)",
        value, QDateTime::fromMSecsSinceEpoch(value, QTimeZone::UTC));

    d->positionMs = d->lastSeekTimeMs = value;

    if (d->archiveReader)
    {
        const qint64 valueUsec = msecToUsec(value);
        qint64 actualPositionUsec = valueUsec;
        bool autoJumpToNearestChunk = playbackState() != State::Previewing;
        if (d->autoJumpPolicy == AutoJumpPolicy::DisableJumpToNextChunk
            ||d->autoJumpPolicy == AutoJumpPolicy::DisableJumpToLiveAndNextChunk)
        {
            autoJumpToNearestChunk = false;
        }
        d->archiveReader->jumpToEx(valueUsec, valueUsec, autoJumpToNearestChunk, &actualPositionUsec);

        const qint64 actualJumpPositionMsec = usecToMsec(actualPositionUsec);
        if (actualJumpPositionMsec != value)
        {
            d->setLiveMode(value == kLivePosition);
            emit positionChanged();

            d->positionMs = d->lastSeekTimeMs = value = actualJumpPositionMsec;
        }
    }

    d->setLiveMode(value == kLivePosition);
    d->setMediaStatus(MediaStatus::Loading);
    d->clearCurrentFrame();
    d->at_hurryUp(); //< renew receiving frames

    emit positionChanged();
}

double Player::speed() const
{
    return d->speed;
}

void Player::setSpeed(double value)
{
    d->speed = value;
    if (d->dataConsumer)
    {
        d->dataConsumer->setPlaySpeed(value);
        d->updateAudio();
    }

    emit speedChanged();
}

int Player::maxTextureSize() const
{
    return d->maxTextureSize;
}

void Player::setMaxTextureSize(int value)
{
    d->maxTextureSize = value;
}

bool Player::checkReadyToPlay()
{
    if (d->archiveReader || d->initDataProvider())
        return true;

    NX_DEBUG(this, "play() END: no data");
    return false;
}

void Player::play()
{
    NX_DEBUG(this, "play() BEGIN");

    if (d->state == State::Playing)
    {
        NX_DEBUG(this, "play() END: already playing");
        return;
    }

    if (!checkReadyToPlay())
        return;

    d->setState(State::Playing);

    d->setMediaStatus(MediaStatus::Loading);
    d->updateAudio();

    d->lastVideoPtsMs.reset();
    d->at_hurryUp(); //< renew receiving frames

    NX_DEBUG(this, "play() END");
}

void Player::pause()
{
    checkReadyToPlay(); //< We need to be sure we are able to show paused frame on camera switch.
    NX_DEBUG(this, "pause()");
    d->setState(State::Paused);
    d->execTimer->stop(); //< stop next frame displaying
    d->updateAudio();
}

void Player::preview()
{
    NX_DEBUG(this, "preview()");

    if (!checkReadyToPlay())
        return;

    d->setState(State::Previewing);
    d->updateAudio();
}

void Player::stop()
{
    NX_DEBUG(this, "stop() BEGIN");

    if (d->archiveReader && d->dataConsumer)
        d->archiveReader->removeDataProcessor(d->dataConsumer.get());
    if (d->dataConsumer)
        d->dataConsumer->pleaseStop();

    d->dataConsumer.reset();
    if (d->archiveReader)
    {
        if (auto context = nx::vms::common::appContext(); NX_ASSERT(context))
            context->longRunableCleanup()->cleanupAsync(std::move(d->archiveReader));
        else
            d->archiveReader.reset();
    }
    d->clearCurrentFrame();
    d->updateCurrentResolution(QSize());

    d->setState(State::Stopped);
    if (d->mediaStatus != MediaStatus::NoVideoStreams) //< Preserve NoVideoStreams state.
        d->setMediaStatus(MediaStatus::NoMedia);

    d->clearVideoOutput();

    NX_DEBUG(this, "stop() END");
}

void Player::setVideoSurface(QVideoSink* videoSurface, int channel)
{
    if (d->videoSurfaces.value(channel) == videoSurface)
        return;

    d->videoSurfaces[channel] = videoSurface;

    if (channel == 0)
        emit videoSurfaceChanged();
}

void Player::unsetVideoSurface(QVideoSink* videoSurface, int channel)
{
    if (d->videoSurfaces.value(channel) != videoSurface)
        return;

    d->videoSurfaces[channel] = nullptr;

    if (channel == 0)
        emit videoSurfaceChanged();
}

bool Player::isAudioEnabled() const
{
    return d->isAudioEnabled;
}

bool Player::isAudioOnlyMode() const
{
    return d->isAudioOnlyMode;
}

void Player::setAudioEnabled(bool value)
{
    if (d->isAudioEnabled != value)
    {
        d->isAudioEnabled = value;
        d->updateAudio();
        emit audioEnabledChanged();
    }
}

bool Player::tooManyConnectionsError() const
{
    return d->lastMediaEvent == nx::media::StreamEvent::tooManyOpenedConnections;
}

bool Player::cannotDecryptMediaError() const
{
    return d->lastMediaEvent == nx::media::StreamEvent::cannotDecryptMedia;
}

QString Player::tag() const
{
    return d->tag;
}

void Player::setTag(const QString& value)
{
    if (d->tag == value)
        return;

    d->tag = value;
    emit tagChanged();
}

bool Player::liveMode() const
{
    return d->liveMode;
}

double Player::aspectRatio() const
{
    return d->aspectRatio;
}

bool Player::reconnectOnPlay() const
{
    return d->reconnectOnPlay;
}

void Player::setReconnectOnPlay(bool reconnectOnPlay)
{
    d->reconnectOnPlay = reconnectOnPlay;
}

int Player::videoQuality() const
{
    return d->videoQuality;
}

void Player::setVideoQuality(int videoQuality)
{
    if (ini().forceIframesOnly && videoQuality == LowVideoQuality)
    {
        NX_DEBUG(this, "setVideoQuality(%1): .ini forceIframesOnly is set => use value %2",
            videoQuality, LowIframesOnlyVideoQuality);
        videoQuality = LowIframesOnlyVideoQuality;
    }

    if (d->videoQuality == videoQuality)
    {
        NX_DEBUG(this, "setVideoQuality(%1): no change, ignoring", videoQuality);
        return;
    }
    NX_DEBUG(this, "setVideoQuality(%1) BEGIN", videoQuality);
    d->videoQuality = videoQuality;
    d->applyVideoQuality();
    emit videoQualityChanged();
    NX_DEBUG(this, "setVideoQuality(%1) END", videoQuality);
}

Player::VideoQuality Player::actualVideoQuality() const
{
    if (!d->archiveReader)
        return HighVideoQuality;

    const auto quality = d->archiveReader->getQuality();
    switch (quality)
    {
        case MEDIA_Quality_Low:
            return LowVideoQuality;

        case MEDIA_Quality_LowIframesOnly:
            return LowIframesOnlyVideoQuality;

        case MEDIA_Quality_CustomResolution:
            return CustomVideoQuality;

        case MEDIA_Quality_High:
        case MEDIA_Quality_ForceHigh:
        default:
            return HighVideoQuality;
    }
}

QList<int> Player::availableVideoQualities(const QList<int>& videoQualities) const
{
    NX_DEBUG(this, "availableVideoQualities() BEGIN");

    QList<int> result;

    const auto transcodingCodec = d->archiveReader
        ? d->archiveReader->getTranscodingCodec()
        : QnArchiveStreamReader(d->resource).getTranscodingCodec();

    const auto camera = d->resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera || !camera->hasVideo())
        return result; //< Setting videoQuality for files is not supported.

    const auto currentDecoders = d->dataConsumer
        ? d->dataConsumer->currentVideoDecoders()
        : std::vector<AbstractVideoDecoder*>();
    const auto getQuality =
        [input = media_player_quality_chooser::Params{
            transcodingCodec,
            d->liveMode,
            d->positionMs,
            camera,
            /*allowOverlay*/ true,
            d->allowHardwareAcceleration,
            &currentDecoders
        }](int quality)
        {
            return media_player_quality_chooser::chooseVideoQuality(quality, input);
        };

    const auto& highQuality = getQuality(HighVideoQuality);
    const auto& maximumResolution = highQuality.frameSize;

    bool customResolutionAvailable = false;
    QualityInfoList customQualities;

    for (auto videoQuality: videoQualities)
    {
        switch (videoQuality)
        {
            case LowIframesOnlyVideoQuality:
            case LowVideoQuality:
                if (getQuality(videoQuality).quality == videoQuality)
                    result.append(videoQuality);
                break;

            case HighVideoQuality:
                if (highQuality.quality == videoQuality)
                    result.append(videoQuality);
                break;

            default:
            {
                auto resultQuality = getQuality(videoQuality);
                if (resultQuality.quality != UnknownVideoQuality
                    && resultQuality.frameSize.height() <= maximumResolution.height())
                {
                    if (resultQuality.quality == CustomVideoQuality)
                        customResolutionAvailable = true;

                    resultQuality.quality = static_cast<VideoQuality>(videoQuality);
                    customQualities.append(resultQuality);
                }
            }
        }
    }

    NX_DEBUG(this, "availableVideoQualities() END");

    if (customResolutionAvailable)
    {
        const auto uniqueCustomQualities = sortOutEqualQualities(customQualities);
        return result + uniqueCustomQualities;
    }

    return result;
}

bool Player::allowOverlay() const
{
    return d->allowOverlay;
}

void Player::setAllowOverlay(bool allowOverlay)
{
    if (d->allowOverlay == allowOverlay)
    {
        NX_DEBUG(this, "setAllowOverlay(%1): no change, ignoring", allowOverlay);
        return;
    }
    NX_DEBUG(this, "setAllowOverlay(%1)", allowOverlay);
    d->allowOverlay = allowOverlay;
    emit allowOverlayChanged();
}

QSize Player::currentResolution() const
{
    return d->currentResolution;
}

Player::TranscodingSupportStatus Player::transcodingStatus() const
{
    const auto& camera = d->resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return TranscodingNotSupported;

    return media_player_quality_chooser::transcodingSupportStatus(
        camera, d->positionMs, d->liveMode);
}

QRect Player::videoGeometry() const
{
    QMutexLocker lock(&d->videoGeometryMutex);
    return d->videoGeometry;
}

void Player::setVideoGeometry(const QRect& rect)
{
    {
        QMutexLocker lock(&d->videoGeometryMutex);

        if (d->videoGeometry == rect)
            return;

        d->videoGeometry = rect;
    }

    emit videoGeometryChanged();
}

PlayerStatistics Player::currentStatistics() const
{
    PlayerStatistics result;

    if (!d->archiveReader)
        return result;

    int channelCount = 1;
    if (auto camera = d->resource.dynamicCast<QnVirtualCameraResource>())
        channelCount = camera->getVideoLayout()->channelCount();

    for (int i = 0; i < channelCount; i++)
    {
        const auto statistics = d->archiveReader->getStatistics(i);
        result.framerate = qMax(result.framerate, static_cast<qreal>(statistics->getFrameRate()));
        result.bitrate += statistics->bitrateBitsPerSecond() / 1024.0 / 1024.0;
    }

    if (const auto codecContext = d->archiveReader->getCodecContext())
        result.codec = codecContext->getCodecName();
    result.isHwAccelerated = d->isHwAccelerated;
    return result;
}

RenderContextSynchronizerPtr Player::renderContextSynchronizer() const
{
    return d->renderContextSynchronizer;
}

void Player::setRenderContextSynchronizer(RenderContextSynchronizerPtr value)
{
    d->renderContextSynchronizer = value;
}

void Player::testSetOwnedArchiveReader(QnArchiveStreamReader* archiveReader)
{
    d->archiveReader.reset(archiveReader);
}

bool Player::addMetadataConsumer(const AbstractMetadataConsumerPtr& metadataConsumer)
{
    if (!metadataConsumer)
        return false;

    auto& consumers = d->m_metadataConsumerByType[metadataConsumer->metadataType()];
    if (consumers.contains(metadataConsumer))
        return false;

    consumers.push_back(metadataConsumer);
    d->configureMetadataForReader();
    return true;
}

bool Player::removeMetadataConsumer(const AbstractMetadataConsumerPtr& metadataConsumer)
{
    if (!metadataConsumer)
        return false;

    auto& consumers = d->m_metadataConsumerByType[metadataConsumer->metadataType()];
    const auto index = consumers.indexOf(metadataConsumer);
    if (index == -1)
        return false;

    consumers.removeAt(index);
    d->configureMetadataForReader();
    return true;
}

void Player::setPlaybackMask(const QnTimePeriodList& periods)
{
    if (d->periods == periods)
        return;

    d->periods = periods;
    if (!d->archiveReader)
        return;

    const auto lastPosition = position();
    d->archiveReader->setPlaybackMask(periods);
    if (!periods.empty() && !periods.containTime(lastPosition))
        setPosition(lastPosition); //< Tries to update position according to the new playback mask.
}

void Player::setPlaybackMask(qint64 startTimeMs, qint64 durationMs)
{
    setPlaybackMask({QnTimePeriod(startTimeMs, durationMs)});
}

void Player::setPlaybackMask(const QVariantList& periods)
{
    static const QString kStartTimeMs = "startTimeMs";
    static const QString kDurationMs = "durationMs";

    QnTimePeriodList result;
    for (const auto& periodItem: periods)
    {
        if (const auto period = periodItem.toJsonObject(); !period.isEmpty())
        {
            if (period.contains(kStartTimeMs) && period.contains(kDurationMs))
            {
                result.includeTimePeriod(
                    {period[kStartTimeMs].toInteger(), period[kDurationMs].toInteger()});
            }
        }
    }
    setPlaybackMask(result);
}

QString videoQualityToString(int videoQuality)
{
    if (videoQuality >= Player::CustomVideoQuality)
    {
        return nx::format("%1 [h: %2]").args(Player::CustomVideoQuality, videoQuality);
    }
    return nx::toString(videoQuality);
}

bool Player::allowHardwareAcceleration() const
{
    return d->allowHardwareAcceleration;
}

void Player::setAllowHardwareAcceleration(bool value)
{
    if (d->allowHardwareAcceleration == value)
        return;

    if (d->dataConsumer)
        d->dataConsumer->setAllowHardwareAcceleration(value);

    d->allowHardwareAcceleration = value;
    emit allowHardwareAccelerationChanged();
}

void Player::invalidateServer()
{
    if (!d->archiveReader)
        return;

    auto delegate = dynamic_cast<QnRtspClientArchiveDelegate*>(
        d->archiveReader->getArchiveDelegate());

    if (!delegate)
        return;
    delegate->invalidateServer();
}

} // namespace media
} // namespace nx
