#include "media_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtCore/QElapsedTimer>
#include <QtOpenGL/QGL>
#include <QtCore/QTimer>

#include <utils/common/delayed.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>

#include "player_data_consumer.h"
#include "frame_metadata.h"
#include "video_decoder_registry.h"
#include "audio_output.h"

#include <plugins/resource/avi/avi_resource.h>
#include <plugins/resource/avi/avi_archive_delegate.h>
#include <nx/utils/flag_config.h>

#define OUTPUT_PREFIX "media_player: "
#include <nx/utils/debug_utils.h>

namespace nx {
namespace media {

namespace {

// Max allowed frame duration. If the distance is higher, then the discontinuity is detected.
static const qint64 kMaxFrameDurationMs = 1000 * 5;

static const qint64 kLivePosition = -1;

// Resync playback timer if a video frame is too late.
static const int kMaxDelayForResyncMs = 500;

// Max allowed amount of underflow/overflow issues in Live mode before extending live buffer.
static const int kMaxCounterForWrongLiveBuffer = 2;

// Calculate next time to render later. It used for AV sync in case of audio buffer has hole in the middle.
// At this case current audio playback position may be significant less than video frame PTS.
// Audio and video timings will became similar as soon as audio buffer passes a hole.
static const int kTryLaterIntervalMs = 16;

// Default value for max openGL texture size
static const int kDefaultMaxTextureSize = 2048;

struct NxMediaFlagConfig: public nx::utils::FlagConfig
{
    using nx::utils::FlagConfig::FlagConfig;

    NX_STRING_PARAM("", substitutePlayerUrl, "Use this Url for video, e.g. file:///c:/test.MP4");
    NX_FLAG(0, outputFrameDelays, "Log if frame delay is negative.");
    NX_FLAG(0, enableFps, "");
};
NxMediaFlagConfig conf("nx_media");

static qint64 msecToUsec(qint64 posMs)
{
    return posMs == kLivePosition ? DATETIME_NOW : posMs * 1000ll;
}

static qint64 usecToMsec(qint64 posUsec)
{
    return posUsec == DATETIME_NOW ? kLivePosition : posUsec / 1000ll;
}

} // namespace

class PlayerPrivate: public QObject
{
    Q_DECLARE_PUBLIC(Player)
    Player *q_ptr;

public:
    // Holds QT property value.
    Player::State state;

    // Holds QT property value.
    Player::MediaStatus mediaStatus;

    // Either media is on live or archive position. Holds QT property value.
    bool liveMode;

    // Auto reconnect if network error
    bool reconnectOnPlay;

    // UTC Playback position at msec. Holds QT property value.
    qint64 positionMs;

    // Video surface to render. Holds QT property value.
    QAbstractVideoSurface* videoSurface;

    // Media URL to play. Holds QT property value.
    QUrl url;

    // Whether the current Media URL refers to a local file.
    bool isLocalFile;

    // Resource obtained for the current Media URL.
    QnResourcePtr resource;

    int maxTextureSize;

    // Main AV timer for playback.
    QElapsedTimer ptsTimer;

    // Last video frame PTS.
    boost::optional<qint64> lastVideoPtsMs;

    // Timestamp when the PTS timer was started.
    qint64 ptsTimerBaseMs;

    // Decoded video which is awaiting to be rendered.
    QVideoFramePtr videoFrameToRender;

    // Separate thread. Performs network IO and gets compressed AV data.
    std::unique_ptr<QnArchiveStreamReader> archiveReader;

    // Separate thread. Decodes compressed AV data.
    std::unique_ptr<PlayerDataConsumer> dataConsumer;

    // Timer for delayed call to presentNextFrame().
    QTimer* execTimer;

    // Last seek position. UTC time in msec.
    qint64 lastSeekTimeMs;

    // Current duration of live buffer in range [kInitialLiveBufferMs.. kMaxLiveBufferMs].
    int liveBufferMs;

    enum class BufferState
    {
        NoIssue,
        Underflow,
        Overflow,
    };

    // Live buffer state for the last frame.
    BufferState liveBufferState;

    // Live buffer underflow counter.
    int underflowCounter;

    // Live buffer overflow counter.
    int overflowCounter;

    // See property comment.
    int videoQuality;

    void applyVideoQuality();

private:
    PlayerPrivate(Player* parent);

    void at_hurryUp();
    void at_gotVideoFrame();
    void presentNextFrameDelayed();

    void presentNextFrame();
    qint64 getDelayForNextFrameWithAudioMs(const QVideoFramePtr& frame);
    qint64 getDelayForNextFrameWithoutAudioMs(const QVideoFramePtr& frame);
    bool createArchiveReader();
    bool initDataProvider();

    void setState(Player::State state);
    void setMediaStatus(Player::MediaStatus status);
    void setLiveMode(bool value);
    void setPosition(qint64 value);

    void resetLiveBufferState();
    void updateLiveBufferState(BufferState value);

    QVideoFramePtr scaleFrame(const QVideoFramePtr& videoFrame);

    bool isTranscodingSupported(const QnVirtualCameraResourcePtr& camera);
    void applyTranscodingIfPossible(
        const QnVirtualCameraResourcePtr& camera, const QSize& resolution);

    void doApplyVideoQuality(const QnVirtualCameraResourcePtr& camera,
        QSize highResolution, CodecID highCodec, QSize lowResolution, CodecID lowCodec);
};

PlayerPrivate::PlayerPrivate(Player *parent)
:
    QObject(parent),
    q_ptr(parent),
    state(Player::State::Stopped),
    mediaStatus(Player::MediaStatus::NoMedia),
    liveMode(true),
    reconnectOnPlay(false),
    positionMs(0),
    videoSurface(0),
    maxTextureSize(kDefaultMaxTextureSize),
    ptsTimerBaseMs(0),
    execTimer(new QTimer(this)),
    lastSeekTimeMs(AV_NOPTS_VALUE),
    liveBufferMs(kInitialBufferMs),
    liveBufferState(BufferState::NoIssue),
    underflowCounter(0),
    overflowCounter(0),
    videoQuality(Player::VideoQuality::High)
{
    connect(execTimer, &QTimer::timeout, this, &PlayerPrivate::presentNextFrame);
    execTimer->setSingleShot(true);
}

void PlayerPrivate::setState(Player::State state)
{
    if (state == this->state)
        return;

    this->state = state;

    Q_Q(Player);
    emit q->playbackStateChanged();
}

void PlayerPrivate::setMediaStatus(Player::MediaStatus status)
{
    if (mediaStatus == status)
        return;

    mediaStatus = status;

    Q_Q(Player);
    emit q->mediaStatusChanged();
}

void PlayerPrivate::setLiveMode(bool value)
{
    if (liveMode == value)
        return;

    liveMode = value;
    Q_Q(Player);
    emit q->liveModeChanged();
}

void PlayerPrivate::setPosition(qint64 value)
{
    if (positionMs == value)
        return;

    positionMs = value;
    Q_Q(Player);
    emit q->positionChanged();
}

void PlayerPrivate::at_hurryUp()
{
    if (videoFrameToRender)
    {
        execTimer->stop();
        presentNextFrame();
    }
}

void PlayerPrivate::at_gotVideoFrame()
{
    if (state == Player::State::Stopped)
        return;

    if (videoFrameToRender)
        return; //< We already have a frame to render. Ignore next frame (will be processed later).

    videoFrameToRender = dataConsumer->dequeueVideoFrame();
    if (!videoFrameToRender)
        return;

    if (state == Player::State::Paused)
    {
        FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);
        if (!metadata.noDelay)
            return; //< Display regular frames only if the player is playing.
    }

    presentNextFrameDelayed();
}

void PlayerPrivate::presentNextFrameDelayed()
{
    if (!videoFrameToRender || !dataConsumer)
        return;

    qint64 delayToRenderMs = 0;
    if (dataConsumer->audioOutput())
    {
        if (dataConsumer->audioOutput()->isBufferUnderflow())
        {
            // If audio buffer is empty we have to display current video frame to unblock data stream
            // and allow audio data to fill the buffer.
            presentNextFrame();
            return;
        }

        delayToRenderMs = getDelayForNextFrameWithAudioMs(videoFrameToRender);

        // If video delay interval is bigger then audio buffer, it'll block audio playing.
        // At this case calculate time again after a delay.
        if (delayToRenderMs > dataConsumer->audioOutput()->currentBufferSizeUsec() / 1000)
        {
            QTimer::singleShot(kTryLaterIntervalMs, this, &PlayerPrivate::presentNextFrameDelayed); //< calculate next time to render later
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

QVideoFramePtr PlayerPrivate::scaleFrame(const QVideoFramePtr& videoFrame)
{
    if (videoFrame->width() <= maxTextureSize && videoFrame->height() <= maxTextureSize)
        return videoFrame; //< Scale is not required.

    QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(
        videoFrame->pixelFormat());
    videoFrame->map(QAbstractVideoBuffer::ReadOnly);
    QImage img(
        videoFrame->bits(),
        videoFrame->width(),
        videoFrame->height(),
        videoFrame->bytesPerLine(),
        imageFormat);
    QVideoFrame* scaledFrame = new QVideoFrame(
        img.scaled(maxTextureSize, maxTextureSize, Qt::KeepAspectRatio));
    videoFrame->unmap();

    scaledFrame->setStartTime(videoFrame->startTime());
    return QVideoFramePtr(scaledFrame);
}

void PlayerPrivate::presentNextFrame()
{
    NX_SHOW_FPS("presentNextFrame");

    if (!videoFrameToRender)
        return;

    setMediaStatus(Player::MediaStatus::Loaded);

    // Update video surface's pixel format if needed.
    if (videoSurface)
    {
        if (videoSurface->isActive() &&
            videoSurface->surfaceFormat().pixelFormat() != videoFrameToRender->pixelFormat())
        {
            videoSurface->stop();
        }

        if (!videoSurface->isActive())
        {
            QVideoSurfaceFormat format(
                videoFrameToRender->size(),
                videoFrameToRender->pixelFormat(),
                videoFrameToRender->handleType());
            videoSurface->start(format);
        }
    }

    if (videoSurface && videoSurface->isActive())
        videoSurface->present(*scaleFrame(videoFrameToRender));

    setPosition(videoFrameToRender->startTime());

    auto metadata = FrameMetadata::deserialize(videoFrameToRender);
    setLiveMode(metadata.flags & QnAbstractMediaData::MediaFlags_LIVE);

    videoFrameToRender.reset();

    // Calculate next time to render.
    QTimer::singleShot(0, this, &PlayerPrivate::at_gotVideoFrame);
}

void PlayerPrivate::resetLiveBufferState()
{
    overflowCounter = underflowCounter = 0;
    liveBufferState = BufferState::NoIssue;
    liveBufferMs = kInitialBufferMs;
}

void PlayerPrivate::updateLiveBufferState(BufferState value)
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

qint64 PlayerPrivate::getDelayForNextFrameWithAudioMs(const QVideoFramePtr& frame)
{
    const AudioOutput* audioOutput = dataConsumer->audioOutput();
    const qint64 currentPosUsec = audioOutput->playbackPositionUsec();
    if (currentPosUsec == AudioOutput::kUnknownPosition)
        return 0; //< Position isn't known yet. Play video without delay.

    qint64 delayToAudioMs = frame->startTime() - currentPosUsec / 1000;
    return delayToAudioMs;
}

qint64 PlayerPrivate::getDelayForNextFrameWithoutAudioMs(const QVideoFramePtr& frame)
{
    const qint64 ptsMs = frame->startTime();
    const qint64 ptsDeltaMs = ptsMs - ptsTimerBaseMs;
    FrameMetadata metadata = FrameMetadata::deserialize(frame);

    // Calculate time to present next frame
    qint64 mediaQueueLenMs = usecToMsec(dataConsumer->queueVideoDurationUsec());
    const qint64 elapsedMs = ptsTimer.elapsed();
    const qint64 frameDelayMs = ptsDeltaMs - elapsedMs;
    bool liveBufferUnderflow =
        liveMode && lastVideoPtsMs.is_initialized() && mediaQueueLenMs == 0 && frameDelayMs < 0;
    bool liveBufferOverflow = liveMode && mediaQueueLenMs > liveBufferMs;

    if (conf.outputFrameDelays)
    {
        if (frameDelayMs < 0)
        {
            PRINT << "ptsMs: " << ptsMs << ", ptsDeltaMs: " << ptsDeltaMs
                << ", frameDelayMs: " << frameDelayMs;
        }
    }

    if (liveMode)
    {
        if (metadata.noDelay)
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

    if (!lastVideoPtsMs.is_initialized() || //< first time
        !qBetween(*lastVideoPtsMs, ptsMs, *lastVideoPtsMs + kMaxFrameDurationMs) || //< pts discontinuity
        metadata.noDelay || //< jump occurred
        ptsMs < lastSeekTimeMs || //< 'coarse' frame. Frame time is less than required jump pos.
        frameDelayMs < -kMaxDelayForResyncMs || //< Resync because the video frame is late for more than threshold.
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
        return (ptsMs - ptsTimerBaseMs) - elapsedMs;
    }
}

void PlayerPrivate::applyVideoQuality()
{
    if (!archiveReader)
        return;

    auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return; //< Setting videoQuality for files is not supported.

    // Obtain Low and High stream codec and resolution.
    QSize highResolution;
    CodecID highCodec = CODEC_ID_NONE;
    QSize lowResolution;
    CodecID lowCodec = CODEC_ID_NONE;
    for (const auto& stream: camera->mediaStreams().streams)
    {
        if (stream.encoderIndex == CameraMediaStreamInfo::PRIMARY_STREAM_INDEX) //< High
        {
            highCodec = (CodecID) stream.codec;
            highResolution = stream.getResolution();
        }
        else if (stream.encoderIndex == CameraMediaStreamInfo::SECONDARY_STREAM_INDEX) //< Low
        {
            lowCodec = (CodecID) stream.codec;
            lowResolution = stream.getResolution();
        }
    }

    doApplyVideoQuality(camera, highResolution, highCodec, lowResolution, lowCodec);
}

namespace {

static const QSize kMaxTranscodingResolution(1920, 1080);

static QSize limitResolution(const QSize& desiredResolution, const QSize& limit)
{
    QSize result = desiredResolution;

    if (result.width() > limit.width())
    {
        result.setHeight((result.height() * limit.width()) / result.width());
        result.setWidth(limit.width());
    }

    if (result.height() > limit.height())
    {
        result.setWidth((result.width() * limit.height()) / result.height());
        result.setHeight(limit.height());
    }

    NX_ASSERT(result.width() <= limit.width());
    NX_ASSERT(result.height() <= limit.height());

    return result;
}

static QSize resolutionWithHeightAndAspect(int height, const QSize& aspect)
{
    return QSize((height * aspect.width()) / aspect.height(), height);
}

} // namespace

void PlayerPrivate::applyTranscodingIfPossible(
    const QnVirtualCameraResourcePtr& camera, const QSize& resolution)
{
    if (isTranscodingSupported(camera))
    {
        QSize transcodingResolution = limitResolution(resolution, kMaxTranscodingResolution);

        if (VideoDecoderRegistry::instance()->hasCompatibleDecoder(
            archiveReader->getTranscodingCodec(), transcodingResolution))
        {
            archiveReader->setQuality(MEDIA_Quality_CustomResolution, /*fastSwitch*/ true,
                transcodingResolution);
        }
    }
}

/**
 * @param highResolution Can be empty.
 * @param highCodec Can be CODEC_ID_NONE.
 * @param lowResolution Can be empty.
 * @param lowCodec Can be CODEC_ID_NONE.
 */
void PlayerPrivate::doApplyVideoQuality(
    const QnVirtualCameraResourcePtr& camera,
    QSize highResolution, CodecID highCodec, QSize lowResolution, CodecID lowCodec)
{
    Q_UNUSED(lowCodec);

    // Set Low stream as a default, and attempt to override it below if needed.
    archiveReader->setQuality(MEDIA_Quality_Low, /*fastSwitch*/ true);

    const bool highStreamRequested = videoQuality == Player::VideoQuality::High ||
        (!highResolution.isEmpty() && videoQuality == highResolution.height());

    const bool lowStreamRequested = videoQuality == Player::VideoQuality::Low ||
        (!lowResolution.isEmpty() && videoQuality == lowResolution.height());

    if (highStreamRequested)
    {
        if (highCodec != CODEC_ID_NONE && !highResolution.isEmpty()) //< High stream exists.
        {
            if (VideoDecoderRegistry::instance()->hasCompatibleDecoder(highCodec, highResolution))
                archiveReader->setQuality(MEDIA_Quality_High, /*fastSwitch*/ true);
            else
                applyTranscodingIfPossible(camera, highResolution);
        }
    }
    else if (!lowStreamRequested) //< Custom resolution requested; videoQuality is lines count.
    {
        QSize aspect = highResolution;
        if (aspect.isEmpty()) //< High stream resolution is unknown.
            aspect = lowResolution;
        if (aspect.isEmpty()) //< Both Low and High stream resolutions are unknown.
            aspect = QSize(16, 9);

        const QSize& desiredResolution = resolutionWithHeightAndAspect(videoQuality, aspect);
        const QSize& maxResolution =
            VideoDecoderRegistry::instance()->maxResolution(archiveReader->getTranscodingCodec());

        applyTranscodingIfPossible(camera, limitResolution(desiredResolution, maxResolution));
    }
}

bool PlayerPrivate::createArchiveReader()
{
    if (!resource)
        return false;

    archiveReader.reset(new QnArchiveStreamReader(resource));
    QnAbstractArchiveDelegate* archiveDelegate;
    if (isLocalFile)
    {
#ifdef ENABLE_ARCHIVE
        archiveDelegate = new QnAviArchiveDelegate();
#endif
    }
    else
    {
        archiveDelegate = new QnRtspClientArchiveDelegate(archiveReader.get());
    }

    archiveReader->setArchiveDelegate(archiveDelegate);
    return true;
}

bool PlayerPrivate::initDataProvider()
{
    if (!createArchiveReader())
    {
        setMediaStatus(Player::MediaStatus::NoMedia);
        return false;
    }

    applyVideoQuality();
    dataConsumer.reset(new PlayerDataConsumer(archiveReader));

    archiveReader->addDataProcessor(dataConsumer.get());
    connect(dataConsumer.get(), &PlayerDataConsumer::gotVideoFrame,
        this, &PlayerPrivate::at_gotVideoFrame);
    connect(dataConsumer.get(), &PlayerDataConsumer::hurryUp,
        this, &PlayerPrivate::at_hurryUp);
    connect(dataConsumer.get(), &PlayerDataConsumer::onEOF, this,
        [this]()
        {
            setPosition(kLivePosition);
        });

    if (!liveMode)
    {
        // Second arg means precise seek.
        archiveReader->jumpTo(msecToUsec(positionMs), msecToUsec(positionMs));
    }

    dataConsumer->start();
    archiveReader->start();

    return true;
}

bool PlayerPrivate::isTranscodingSupported(const QnVirtualCameraResourcePtr& camera)
{
    if (VideoDecoderRegistry::instance()->isLiteClientMode())
        return false;

    QnMediaServerResourcePtr server = liveMode ? camera->getParentServer()
        : qnCameraHistoryPool->getMediaServerOnTime(camera, positionMs);

    return server->getServerFlags() & Qn::SF_SupportsTranscoding;
}

//-------------------------------------------------------------------------------------------------
// Player

Player::Player(QObject *parent)
:
    QObject(parent),
    d_ptr(new PlayerPrivate(this))
{
    conf.reload();
}

Player::~Player()
{
    stop();
}

Player::State Player::playbackState() const
{
    Q_D(const Player);
    return d->state;
}

Player::MediaStatus Player::mediaStatus() const
{
    Q_D(const Player);
    return d->mediaStatus;
}

QUrl Player::source() const
{
    Q_D(const Player);
    return d->url;
}

QAbstractVideoSurface *Player::videoSurface() const
{
    Q_D(const Player);
    return d->videoSurface;
}

qint64 Player::position() const
{
    Q_D(const Player);
    return d->positionMs;
}

void Player::setPosition(qint64 value)
{
    Q_D(Player);
    d->lastSeekTimeMs = value;
    if (d->archiveReader)
        d->archiveReader->jumpTo(msecToUsec(value), 0);
    else
        d->positionMs = value;

    d->setLiveMode(value == kLivePosition);

    d->at_hurryUp(); //< renew receiving frames
}

int Player::maxTextureSize() const
{
    Q_D(const Player);
    return d->maxTextureSize;
}

void Player::setMaxTextureSize(int value)
{
    Q_D(Player);
    d->maxTextureSize = value;
}

void Player::play()
{
    Q_D(Player);

    if (d->state == State::Playing)
        return;

    if (!d->archiveReader && !d->initDataProvider())
        return;

    d->setState(State::Playing);
    d->setMediaStatus(MediaStatus::Loading);

    d->lastVideoPtsMs.reset();
    d->at_hurryUp(); //< renew receiving frames
}

void Player::pause()
{
    Q_D(Player);
    d->setState(State::Paused);
}

void Player::stop()
{
    Q_D(Player);

    if (d->archiveReader && d->dataConsumer)
        d->archiveReader->removeDataProcessor(d->dataConsumer.get());

    d->dataConsumer.reset();
    d->archiveReader.reset();
    d->setState(State::Stopped);
}

void Player::setSource(const QUrl& url)
{
    Q_D(Player);

    const QUrl& newUrl = *conf.substitutePlayerUrl ? QUrl(conf.substitutePlayerUrl) : url;

    if (newUrl != d->url)
    {
        stop();
        d->url = newUrl;

        const QString path(d->url.path().mid(1));
        d->isLocalFile = d->url.scheme() == lit("file");
        if (d->isLocalFile)
        {
#ifdef ENABLE_ARCHIVE
            d->resource.reset(new QnAviResource(path));
            d->resource->setStatus(Qn::Online);
#else
            d->resource.reset();
#endif
        }
        else
        {
            d->resource = qnResPool->getResourceById(QnUuid(path));
        }
    }
}

void Player::setVideoSurface(QAbstractVideoSurface* videoSurface)
{
    Q_D(Player);

    if (d->videoSurface == videoSurface)
        return;

    d->videoSurface = videoSurface;

    emit videoSurfaceChanged();
}

bool Player::liveMode() const
{
    Q_D(const Player);
    return d->liveMode;
}

bool Player::reconnectOnPlay() const
{
    Q_D(const Player);
    return d->reconnectOnPlay;
}

void Player::setReconnectOnPlay(bool reconnectOnPlay)
{
    Q_D(Player);
    d->reconnectOnPlay = reconnectOnPlay;
}

int Player::videoQuality() const
{
    Q_D(const Player);
    return d->videoQuality;
}

void Player::setVideoQuality(int videoQuality)
{
    Q_D(Player);
    d->videoQuality = videoQuality;
    d->applyVideoQuality();
}

QSize Player::currentResolution() const
{
    Q_D(const Player);
    if (d->dataConsumer)
        return d->dataConsumer->currentResolution();
    else
        return QSize();
}

} // namespace media
} // namespace nx
