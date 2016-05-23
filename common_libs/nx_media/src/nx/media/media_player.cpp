#include "media_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtCore/QElapsedTimer>
#include <QtOpenGL/QGL>
#include <QtCore/QTimer>

#include <utils/common/delayed.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>

#include "player_data_consumer.h"
#include "frame_metadata.h"
#include "video_decoder_registry.h"
#include "audio_output.h"
#include <plugins/resource/avi/avi_resource.h>
#include <plugins/resource/avi/avi_archive_delegate.h>

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

    int maxTextureSize;

    // Main AV timer for playback.
    QElapsedTimer ptsTimer;

    // Last video frame PTS.
    boost::optional<qint64> lastVideoPts;

    // Timestamp when the PTS timer was started.
    qint64 ptsTimerBase;

    // Decoded video which is awaiting to be rendered.
    QVideoFramePtr videoFrameToRender;

    // Separate thread. Performs network IO and gets compressed AV data.
    std::unique_ptr<QnArchiveStreamReader> archiveReader;

    // Separate thread. Decodes compressed AV data.
    std::unique_ptr<PlayerDataConsumer> dataConsumer;

    // Timer for delayed call to presentFrame().
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

    // Video quality
    Player::VideoQuality videoQuality;

    // User-defined video resolution for custom quality
    QSize videoResolution;

    void updateVideoQuality();
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
    ptsTimerBase(0),
    execTimer(new QTimer(this)),
    lastSeekTimeMs(AV_NOPTS_VALUE),
    liveBufferMs(kInitialBufferMs),
    liveBufferState(BufferState::NoIssue),
    underflowCounter(0),
    overflowCounter(0),
    videoQuality(Player::VideoQuality::Auto)
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
        execTimer->start(delayToRenderMs);
    else
        presentNextFrame();
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
    const qint64 pts = frame->startTime();
    const qint64 ptsDelta = pts - ptsTimerBase;
    FrameMetadata metadata = FrameMetadata::deserialize(frame);

    // Calculate time to present next frame
    qint64 mediaQueueLen = usecToMsec(dataConsumer->queueVideoDurationUsec());
    const auto elapsed = ptsTimer.elapsed();
    const int frameDelayMs = ptsDelta - elapsed;
    bool liveBufferUnderflow =
        liveMode && lastVideoPts.is_initialized() && mediaQueueLen == 0 && frameDelayMs < 0;
    bool liveBufferOverflow = liveMode && mediaQueueLen > liveBufferMs;

    // TODO mike: REMOVE
#if 0
    if (frameDelayMs < 0)
        qWarning() << "pts: " << pts << ", ptsDelta: " << ptsDelta << ", frameDelayMs: " << frameDelayMs;
#endif // 0

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

    if (!lastVideoPts.is_initialized() || //< first time
        !qBetween(*lastVideoPts, pts, *lastVideoPts + kMaxFrameDurationMs) || //< pts discontinue
        metadata.noDelay || //< jump occurred
        pts < lastSeekTimeMs || //< 'coarse' frame. Frame time is less than required jump pos.
        frameDelayMs < -kMaxDelayForResyncMs || //< Resync because the video frame is late for more than threshold.
        liveBufferUnderflow ||
        liveBufferOverflow //< live buffer overflow
        )
    {
        // Reset timer.
        lastVideoPts = ptsTimerBase = pts;
        ptsTimer.restart();
        return 0ll;
    }
    else
    {
        lastVideoPts = pts;
        return (pts - ptsTimerBase) - ptsTimer.elapsed();
    }
}

void PlayerPrivate::updateVideoQuality()
{
    if (!archiveReader)
        return;

    if (videoQuality == Player::VideoQuality::Auto)
        archiveReader->setQuality(MEDIA_Quality_High, true); // MEDIA_Quality_Auto
    else if (videoQuality == Player::VideoQuality::High)
        archiveReader->setQuality(MEDIA_Quality_High, true);
    else if (videoQuality == Player::VideoQuality::Low)
        archiveReader->setQuality(MEDIA_Quality_Low, true);
    else if (videoQuality == Player::VideoQuality::Custom)
        archiveReader->setQuality(MEDIA_Quality_CustomResolution, true, videoResolution);
}

bool PlayerPrivate::createArchiveReader()
{
    QString path(url.path().mid(1));
    bool isLocalFile = url.scheme() == lit("file");
    QnResourcePtr resource;
    if (isLocalFile)
    {
        resource = QnAviResourcePtr(new QnAviResource(path));
        resource->setStatus(Qn::Online);
    }
    else
    {
        resource = qnResPool->getResourceById(QnUuid(path));
    }

    if (!resource)
        return false;

    archiveReader.reset(new QnArchiveStreamReader(resource));
    QnAbstractArchiveDelegate* archiveDelegate;
    if (isLocalFile)
        archiveDelegate = new QnAviArchiveDelegate();
    else
        archiveDelegate = new QnRtspClientArchiveDelegate(archiveReader.get());

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

    updateVideoQuality();
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

    auto resource = archiveReader->getResource();
    QnVirtualCameraResourcePtr camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (camera)
    {
        for (const auto& stream: camera->mediaStreams().streams)
        {
            if (stream.encoderIndex != CameraMediaStreamInfo::PRIMARY_STREAM_INDEX)
                continue;
            CodecID codec = (CodecID)stream.codec;
            if (!VideoDecoderRegistry::instance()->hasCompatibleDecoder(
                codec, stream.getResolution()))
            {
                // No compatible decoder for High quality. Force low quality.
                archiveReader->setQuality(MEDIA_Quality_Low, true);
            }
        }
    }

    if (!liveMode)
    {
        // Second arg means precise seek.
        archiveReader->jumpTo(msecToUsec(positionMs), msecToUsec(positionMs));
    }

    dataConsumer->start();
    archiveReader->start();

    return true;
}

//-------------------------------------------------------------------------------------------------
// Player

Player::Player(QObject *parent)
:
    QObject(parent),
    d_ptr(new PlayerPrivate(this))
{
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

    d->lastVideoPts.reset();
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

    if (url == d->url)
        return;

    stop();
    d->url = url;
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

Player::VideoQuality Player::videoQuality() const
{
    Q_D(const Player);
    return d->videoQuality;
}

void Player::setVideoQuality(const VideoQuality& value)
{
    Q_D(Player);

    if (d->videoQuality == value)
        return;

    d->videoQuality = value;
    d->updateVideoQuality();
}

QSize Player::videoResolution() const
{
    Q_D(const Player);
    return d->videoResolution;
}

void Player::setVideoResolution(const QSize& value)
{
    Q_D(Player);
    d->videoResolution = value;;
    d->updateVideoQuality();
}


} // namespace media
} // namespace nx
