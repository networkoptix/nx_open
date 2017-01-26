#include "media_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtCore/QMutex>

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
#include <nx/utils/log/log.h>

#define OUTPUT_PREFIX "media_player: "
#include <nx/utils/debug_utils.h>

#include "media_player_quality_chooser.h"
#include <utils/common/long_runable_cleanup.h>

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

// Player will go to the invalid state if no data is received within this timeout.
static const int kGotDataTimeoutMs = 1000 * 30;

// Periodic tasks timer interval
static const int kPeriodicTasksTimeoutMs = 1000;

struct NxMediaFlagConfig: public nx::utils::FlagConfig
{
    using nx::utils::FlagConfig::FlagConfig;

    NX_STRING_PARAM("", substitutePlayerUrl, "Use this Url for video, e.g. file:///c:/test.MP4");
    NX_FLAG(0, outputFrameDelays, "Log if frame delay is negative.");
    NX_FLAG(0, enableFps, "");
    NX_INT_PARAM(-1, hwVideoX, "If not -1, override hardware video window X.");
    NX_INT_PARAM(-1, hwVideoY, "If not -1, override hardware video window Y.");
    NX_INT_PARAM(-1, hwVideoWidth, "If not -1, override hardware video window width.");
    NX_INT_PARAM(-1, hwVideoHeight, "If not -1, override hardware video window height.");
    NX_FLAG(0, forceIframesOnly, "For Low Quality selection, force I-frames-only mode.");
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

    // Video aspect ratio
    double aspectRatio;

    // Auto reconnect if network error
    bool reconnectOnPlay;

    // UTC Playback position at msec. Holds QT property value.
    qint64 positionMs;

    // Video surface list to render. Holds QT property value.
    QMap<int, QAbstractVideoSurface*> videoSurfaces;

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

    // Timer for miscs periodic tasks
    QTimer* miscTimer;

    // Last seek position. UTC time in msec.
    qint64 lastSeekTimeMs;

    bool waitWhileJumpFinished;

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

    // Video geometry inside the application window.
    QRect videoGeometry;

    // Protects access to videoGeometry.
    mutable QMutex videoGeometryMutex;

    // Interval since last time player got some data
    QElapsedTimer gotDataTimer;

    void applyVideoQuality();

private:
    PlayerPrivate(Player* parent);

    void at_hurryUp();
    void at_jumpOccurred(int sequence);
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
    void setAspectRatio(double value);
    void setPosition(qint64 value);

    void resetLiveBufferState();
    void updateLiveBufferState(BufferState value);

    QVideoFramePtr scaleFrame(const QVideoFramePtr& videoFrame);

    void doPeriodicTasks();

    void log(const QString& message) const;

    bool isCoarseFrame(const QVideoFramePtr& frame) const;
};

PlayerPrivate::PlayerPrivate(Player *parent):
    QObject(parent),
    q_ptr(parent),
    state(Player::State::Stopped),
    mediaStatus(Player::MediaStatus::NoMedia),
    liveMode(true),
    aspectRatio(1.0),
    reconnectOnPlay(false),
    positionMs(0),
    maxTextureSize(kDefaultMaxTextureSize),
    ptsTimerBaseMs(0),
    execTimer(new QTimer(this)),
    miscTimer(new QTimer(this)),
    lastSeekTimeMs(AV_NOPTS_VALUE),
    waitWhileJumpFinished(false),
    liveBufferMs(kInitialBufferMs),
    liveBufferState(BufferState::NoIssue),
    underflowCounter(0),
    overflowCounter(0),
    videoQuality(Player::HighVideoQuality)
{
    connect(execTimer, &QTimer::timeout, this, &PlayerPrivate::presentNextFrame);
    execTimer->setSingleShot(true);

    connect(miscTimer, &QTimer::timeout, this, &PlayerPrivate::doPeriodicTasks);
    miscTimer->start(kPeriodicTasksTimeoutMs);
}

void PlayerPrivate::setState(Player::State state)
{
    if (state == this->state)
        return;

    gotDataTimer.restart();
    this->state = state;

    Q_Q(Player);
    emit q->playbackStateChanged();
}

void PlayerPrivate::doPeriodicTasks()
{
    Q_Q(Player);

    if (state == Player::State::Playing)
    {
        if (dataConsumer &&
            dataConsumer->audioOutput() &&
            dataConsumer->audioOutput()->currentBufferSizeUsec() > 0)
        {
            gotDataTimer.restart();
            return;
        }

        if (gotDataTimer.hasExpired(kGotDataTimeoutMs))
        {
            log("doPeriodicTasks(): No data, timeout expired => setMediaStatus(NoMedia)");
            setMediaStatus(Player::MediaStatus::NoMedia);
            q->stop();
        }
    }
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

void PlayerPrivate::setAspectRatio(double value)
{
    if (qFuzzyEquals(aspectRatio, value))
        return;

    aspectRatio = value;
    Q_Q(Player);
    emit q->aspectRatioChanged();
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

void PlayerPrivate::at_jumpOccurred(int sequence)
{
    waitWhileJumpFinished = false;
    if (!videoFrameToRender)
        return;
    FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);
    if (sequence && sequence != metadata.sequence)
    {
        // Drop deprecate frame
        execTimer->stop();
        videoFrameToRender.reset();
        at_gotVideoFrame();
    }
}

void PlayerPrivate::at_gotVideoFrame()
{
    Q_Q(Player);

    if (state == Player::State::Stopped)
        return;

    if (videoFrameToRender)
        return; //< We already have a frame to render. Ignore next frame (will be processed later).

    videoFrameToRender = dataConsumer->dequeueVideoFrame();
    if (!videoFrameToRender)
        return;

    FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);
    if (metadata.dataType == QnAbstractMediaData::EMPTY_DATA)
    {
        videoFrameToRender.reset();
        q->setPosition(kLivePosition); //< EOF reached
        return;
    }

    if (state == Player::State::Paused)
    {
        if (!metadata.noDelay && !isCoarseFrame(videoFrameToRender))
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
    gotDataTimer.restart();

    FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);

    auto videoSurface = videoSurfaces.value(metadata.videoChannel);


    // Update video surface's pixel format if needed.
    if (videoSurface)
    {
        if (videoSurface->isActive() &&
            (videoSurface->surfaceFormat().pixelFormat() != videoFrameToRender->pixelFormat()
                || videoSurface->surfaceFormat().frameSize() != videoFrameToRender->size()))
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

    bool isLivePacket = metadata.flags.testFlag(QnAbstractMediaData::MediaFlags_LIVE);
    bool isPacketOK = (isLivePacket == liveMode);

    if (videoSurface && videoSurface->isActive() && isPacketOK)
    {
        videoSurface->present(*scaleFrame(videoFrameToRender));
        if (dataConsumer)
        {
            qint64 timeUs = liveMode ? DATETIME_NOW : videoFrameToRender->startTime() * 1000;
            dataConsumer->setDisplayedTimeUs(timeUs);
        }
        bool ignoreFrameTime = metadata.noDelay || waitWhileJumpFinished;
        if (!ignoreFrameTime)
            setPosition(videoFrameToRender->startTime());
        setAspectRatio(videoFrameToRender->width() * metadata.sar / videoFrameToRender->height());
    }
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
        isCoarseFrame(frame) || //< 'coarse' frame. Frame time is less than required jump pos.
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

    const QSize& quality = media_player_quality_chooser::chooseVideoQuality(
        archiveReader->getTranscodingCodec(),
        videoQuality,
        liveMode,
        positionMs,
        camera);

    if (quality == media_player_quality_chooser::kQualityHigh)
    {
        archiveReader->setQuality(MEDIA_Quality_High, /*fastSwitch*/ true);
    }
    else if (quality == media_player_quality_chooser::kQualityLow)
    {
        archiveReader->setQuality(MEDIA_Quality_Low, /*fastSwitch*/ true);
    }
    else if (quality == media_player_quality_chooser::kQualityLowIframesOnly)
    {
        archiveReader->setQuality(MEDIA_Quality_LowIframesOnly, /*fastSwitch*/ true);
    }
    else
    {
        NX_ASSERT(quality.isValid());
        archiveReader->setQuality(MEDIA_Quality_CustomResolution, /*fastSwitch*/ true,
            QSize(/*width*/ 0, quality.height())); //< Use "auto" width.
    }
    at_hurryUp(); //< skip waiting for current frame
}

bool PlayerPrivate::createArchiveReader()
{
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

    applyVideoQuality();
    dataConsumer.reset(new PlayerDataConsumer(archiveReader));
    dataConsumer->setVideoGeometryAccessor(
        [guardedThis = QPointer<PlayerPrivate>(this)]()
        {
            QRect r;
            if (guardedThis)
            {
                QMutexLocker lock(&guardedThis->videoGeometryMutex);
                r = guardedThis->videoGeometry;
            }

            if (conf.hwVideoX != -1)
                r.setX(conf.hwVideoX);
            if (conf.hwVideoY != -1)
                r.setY(conf.hwVideoY);
            if (conf.hwVideoWidth != -1)
                r.setWidth(conf.hwVideoWidth);
            if (conf.hwVideoHeight != -1)
                r.setHeight(conf.hwVideoHeight);

            return r;
        });

    archiveReader->addDataProcessor(dataConsumer.get());
    connect(dataConsumer.get(), &PlayerDataConsumer::gotVideoFrame,
        this, &PlayerPrivate::at_gotVideoFrame);
    connect(dataConsumer.get(), &PlayerDataConsumer::hurryUp,
        this, &PlayerPrivate::at_hurryUp);
    connect(dataConsumer.get(), &PlayerDataConsumer::jumpOccurred,
        this, &PlayerPrivate::at_jumpOccurred);

    if (!liveMode)
    {
        // Second arg means precise seek.
        archiveReader->jumpTo(msecToUsec(positionMs), msecToUsec(positionMs));
    }

    dataConsumer->start();
    archiveReader->start();

    return true;
}

void PlayerPrivate::log(const QString& message) const
{
    NX_LOG(lit("[media_player @%1] %2")
        .arg(reinterpret_cast<uintptr_t>(this), 8, 16, QLatin1Char('0'))
        .arg(message), cl_logDEBUG1);
}

bool PlayerPrivate::isCoarseFrame(const QVideoFramePtr& frame) const
{
    return frame->startTime() < lastSeekTimeMs;
}

//-------------------------------------------------------------------------------------------------
// Player

Player::Player(QObject *parent):
    QObject(parent),
    d_ptr(new PlayerPrivate(this))
{
    Q_D(const Player);
    d->log(lit("Player()"));
    conf.reload();
}

Player::~Player()
{
    Q_D(const Player);
    d->log(lit("~Player() BEGIN"));
    stop();
    d->log(lit("~Player() END"));
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

QAbstractVideoSurface* Player::videoSurface(int channel) const
{
    Q_D(const Player);
    return d->videoSurfaces.value(channel);
}

qint64 Player::position() const
{
    Q_D(const Player);
    // positionMs is the actual value from a video frame. It could contain a "coarse" timestamp
    // a bit less then the user requested position (inside of the current GOP).
    return qMax(d->lastSeekTimeMs, d->positionMs);
}

void Player::setPosition(qint64 value)
{
    Q_D(Player);
    d->log(lit("setPosition(%1)").arg(value));

    d->positionMs = d->lastSeekTimeMs = value;
    if (d->archiveReader)
    {
        d->archiveReader->jumpTo(msecToUsec(value), 0);
        d->waitWhileJumpFinished = true;
    }

    d->setLiveMode(value == kLivePosition);

    d->at_hurryUp(); //< renew receiving frames

    emit positionChanged();
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
    d->log(lit("play() BEGIN"));

    if (d->state == State::Playing)
    {
        d->log(lit("play() END: already playing"));
        return;
    }

    if (!d->archiveReader && !d->initDataProvider())
    {
        d->log(lit("play() END: no data"));
        return;
    }

    d->setState(State::Playing);
    d->setMediaStatus(MediaStatus::Loading);

    d->lastVideoPtsMs.reset();
    d->at_hurryUp(); //< renew receiving frames

    d->log(lit("play() END"));
}

void Player::pause()
{
    Q_D(Player);
    d->log(lit("pause()"));
    d->setState(State::Paused);
}

void Player::stop()
{
    Q_D(Player);
    d->log(lit("stop() BEGIN"));

    if (d->archiveReader && d->dataConsumer)
        d->archiveReader->removeDataProcessor(d->dataConsumer.get());
    if (d->dataConsumer)
        d->dataConsumer->pleaseStop();

    d->dataConsumer.reset();
    if (d->archiveReader)
    {
        if (QnLongRunableCleanup::instance())
            QnLongRunableCleanup::instance()->cleanupAsync(std::move(d->archiveReader));
        else
            d->archiveReader.reset();
    }
    d->videoFrameToRender.reset();

    d->setState(State::Stopped);
    d->log(lit("stop() END"));
}

void Player::setSource(const QUrl& url)
{
    Q_D(Player);

    const QUrl& newUrl = *conf.substitutePlayerUrl ? QUrl(conf.substitutePlayerUrl) : url;

    if (newUrl == d->url)
    {
        d->log(lit("setSource(\"%1\"): no change, ignoring").arg(newUrl.toString()));
        return;
    }

    d->log(lit("setSource(\"%1\") BEGIN").arg(newUrl.toString()));

    const State currentState = d->state;

    stop();
    d->url = newUrl;

    const QString path(d->url.path().mid(1));
    d->isLocalFile = d->url.scheme() == lit("file");
    if (d->isLocalFile)
    {
        d->resource.reset(new QnAviResource(path));
        d->resource->setStatus(Qn::Online);
    }
    else
    {
        d->resource = qnResPool->getResourceById(QnUuid(path));
    }

    if (d->resource && currentState == State::Playing)
        play();

    d->log(lit("emit sourceChanged()"));
    emit sourceChanged();

    d->log(lit("setSource(\"%1\") END").arg(newUrl.toString()));
}

void Player::setVideoSurface(QAbstractVideoSurface* videoSurface, int channel)
{
    Q_D(Player);

    if (d->videoSurfaces.value(channel) == videoSurface)
        return;

    d->videoSurfaces[channel] = videoSurface;

    emit videoSurfaceChanged();
}

bool Player::liveMode() const
{
    Q_D(const Player);
    return d->liveMode;
}

double Player::aspectRatio() const
{
    Q_D(const Player);
    return d->aspectRatio;
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

    if (conf.forceIframesOnly && videoQuality == LowVideoQuality)
    {
        d->log(lit("setVideoQuality(%1): config forceIframesOnlyis true => use value %2")
            .arg(videoQuality).arg(LowIframesOnlyVideoQuality));
        videoQuality = LowIframesOnlyVideoQuality;
    }

    if (d->videoQuality == videoQuality)
    {
        d->log(lit("setVideoQuality(%1): no change, ignoring").arg(videoQuality));
        return;
    }
    d->log(lit("setVideoQuality(%1) BEGIN").arg(videoQuality));
    d->videoQuality = videoQuality;
    d->applyVideoQuality();
    emit videoQualityChanged();
    d->log(lit("setVideoQuality(%1) END").arg(videoQuality));
}

QSize Player::currentResolution() const
{
    Q_D(const Player);
    if (d->dataConsumer)
        return d->dataConsumer->currentResolution();
    else
        return QSize();
}

QRect Player::videoGeometry() const
{
    Q_D(const Player);
    QMutexLocker lock(&d->videoGeometryMutex);
    return d->videoGeometry;
}

void Player::setVideoGeometry(const QRect& rect)
{
    Q_D(Player);

    {
        QMutexLocker lock(&d->videoGeometryMutex);

        if (d->videoGeometry == rect)
            return;

        d->videoGeometry = rect;
    }

    emit videoGeometryChanged();
}

void Player::testSetOwnedArchiveReader(QnArchiveStreamReader* archiveReader)
{
    Q_D(Player);
    d->archiveReader.reset(archiveReader);
}

void Player::testSetCamera(const QnResourcePtr& camera)
{
    Q_D(Player);
    d->resource = camera;
}

} // namespace media
} // namespace nx
