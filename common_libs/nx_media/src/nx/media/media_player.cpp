#include "media_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtCore/QElapsedTimer>
#include <QtCore/QTimer>
#include <QtCore/QMutex>

#include <nx/kit/debug.h>

#include <common/common_module.h>
#include <nx/utils/log/log.h>
#include <utils/common/long_runable_cleanup.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>

#include <core/resource/avi/avi_resource.h>
#include <core/resource/avi/avi_archive_delegate.h>

#include <nx/fusion/model_functions.h>

#include <nx/streaming/archive_stream_reader.h>
#include <nx/streaming/rtsp_client_archive_delegate.h>

#include <nx/media/ini.h>

#include "player_data_consumer.h"
#include "frame_metadata.h"
#include "video_decoder_registry.h"
#include "audio_output.h"

#include "media_player_quality_chooser.h"

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

using QualityInfo = nx::media::media_player_quality_chooser::Result;
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

class PlayerPrivate: public QObject
{
    Q_DECLARE_PUBLIC(Player)
    Player* q_ptr;

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

    bool allowOverlay;

    // Video geometry inside the application window.
    QRect videoGeometry;

    // Resolution of the last displayed frame.
    QSize currentResolution;

    // Protects access to videoGeometry.
    mutable QMutex videoGeometryMutex;

    // Interval since last time player got some data
    QElapsedTimer gotDataTimer;

    // Turn on / turn off audio.
    bool isAudioEnabled;

    // Hardware decoding has been used for the last presented frame.
    bool isHwAccelerated;

    void applyVideoQuality();

private:
    PlayerPrivate(Player* parent);

    void at_hurryUp();
    void at_jumpOccurred(int sequence);
    void at_gotVideoFrame();
    void presentNextFrameDelayed();

    void presentNextFrame();
    qint64 getDelayForNextFrameWithAudioMs(
        const QVideoFramePtr& frame,
        const ConstAudioOutputPtr& audioOutput);
    qint64 getDelayForNextFrameWithoutAudioMs(const QVideoFramePtr& frame);
    bool createArchiveReader();
    bool initDataProvider();

    void setState(Player::State state);
    void setMediaStatus(Player::MediaStatus status);
    void setLiveMode(bool value);
    void setAspectRatio(double value);
    void setPosition(qint64 value);
    void updateCurrentResolution(const QSize& size);

    void resetLiveBufferState();
    void updateLiveBufferState(BufferState value);

    QVideoFramePtr scaleFrame(const QVideoFramePtr& videoFrame);

    void doPeriodicTasks();

    void log(const QString& message) const;
    void clearCurrentFrame();
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
    liveBufferMs(kInitialBufferMs),
    liveBufferState(BufferState::NoIssue),
    underflowCounter(0),
    overflowCounter(0),
    videoQuality(Player::HighVideoQuality),
    allowOverlay(true),
    isAudioEnabled(true),
    isHwAccelerated(false)
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

void PlayerPrivate::updateCurrentResolution(const QSize& size)
{
    if (currentResolution == size)
        return;

    currentResolution = size;
    Q_Q(Player);
    emit q->currentResolutionChanged();
}

void PlayerPrivate::at_hurryUp()
{
    if (videoFrameToRender)
    {
        execTimer->stop();
        presentNextFrame();
    }
    else
    {
        QTimer::singleShot(0, this, &PlayerPrivate::at_gotVideoFrame);
    }
}

void PlayerPrivate::at_jumpOccurred(int sequence)
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

void PlayerPrivate::at_gotVideoFrame()
{
    Q_Q(Player);

    if (state == Player::State::Stopped)
        return;

    if (videoFrameToRender)
        return; //< We already have a frame to render. Ignore next frame (will be processed later).

    if (!dataConsumer)
        return;

    videoFrameToRender = dataConsumer->dequeueVideoFrame();
    if (!videoFrameToRender)
        return;

    FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);
    if (metadata.dataType == QnAbstractMediaData::EMPTY_DATA)
    {
        videoFrameToRender.reset();
        log("at_gotVideoFrame(): EOF reached, jumping to LIVE.");
        q->setPosition(kLivePosition);
        return;
    }

    if (state == Player::State::Paused || state == Player::State::Previewing)
    {
        if (metadata.displayHint == DisplayHint::regular)
            return; //< Display regular frames if and only if the player is playing.
    }

    presentNextFrameDelayed();
}

void PlayerPrivate::presentNextFrameDelayed()
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
    NX_FPS(PresentNextFrame);

    if (!videoFrameToRender)
        return;

    gotDataTimer.restart();

    updateCurrentResolution(videoFrameToRender->size());

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
    bool skipFrame = isLivePacket != liveMode
        || (state != Player::State::Previewing
            && metadata.displayHint == DisplayHint::noDelay);

    if (videoSurface && videoSurface->isActive() && !skipFrame)
    {
        setMediaStatus(Player::MediaStatus::Loaded);
        isHwAccelerated = metadata.flags.testFlag(QnAbstractMediaData::MediaFlags_HWDecodingUsed);
        videoSurface->present(*scaleFrame(videoFrameToRender));
        if (dataConsumer)
        {
            qint64 timeUs = liveMode ? DATETIME_NOW : videoFrameToRender->startTime() * 1000;
            dataConsumer->setDisplayedTimeUs(timeUs);
        }
        if (metadata.displayHint != DisplayHint::noDelay)
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

qint64 PlayerPrivate::getDelayForNextFrameWithAudioMs(
    const QVideoFramePtr& frame,
    const ConstAudioOutputPtr& audioOutput)
{
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

    if (!lastVideoPtsMs.is_initialized() || //< first time
        !qBetween(*lastVideoPtsMs, ptsMs, *lastVideoPtsMs + kMaxFrameDurationMs) || //< pts discontinuity
        metadata.displayHint != DisplayHint::regular || //< jump occurred
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
    Q_Q(Player);

    if (!archiveReader)
        return;

    auto camera = resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return; //< Setting videoQuality for files is not supported.

    const auto currentVideoDecoders = dataConsumer
        ? dataConsumer->currentVideoDecoders()
        : std::vector<AbstractVideoDecoder*>();

    const auto& result = media_player_quality_chooser::chooseVideoQuality(
        archiveReader->getTranscodingCodec(),
        videoQuality,
        liveMode,
        positionMs,
        camera,
        allowOverlay,
        currentVideoDecoders);

    switch (result.quality)
    {
        case Player::UnknownVideoQuality:
            log("applyVideoQuality(): Could not choose quality => setMediaStatus(NoVideoStreams)");
            setMediaStatus(Player::MediaStatus::NoVideoStreams);
            q->stop();
            return;

        case Player::HighVideoQuality:
            archiveReader->setQuality(MEDIA_Quality_High, /*fastSwitch*/ true);
            break;

        case Player::LowVideoQuality:
            archiveReader->setQuality(MEDIA_Quality_Low, /*fastSwitch*/ true);
            break;

        case Player::LowIframesOnlyVideoQuality:
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

    if (!archiveReader)
        return false;

    dataConsumer.reset(new PlayerDataConsumer(archiveReader));
    dataConsumer->setAudioEnabled(isAudioEnabled);
    dataConsumer->setAllowOverlay(allowOverlay);

    dataConsumer->setVideoGeometryAccessor(
        [guardedThis = QPointer<PlayerPrivate>(this)]()
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

void PlayerPrivate::clearCurrentFrame()
{
    execTimer->stop();
    videoFrameToRender.reset();
}

//-------------------------------------------------------------------------------------------------
// Player

Player::Player(QObject *parent):
    QObject(parent),
    d_ptr(new PlayerPrivate(this))
{
    Q_D(const Player);
    d->log(lit("Player()"));
    ini().reload();
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
    d->log(lit("setPosition(%1: %2)")
        .arg(value)
        .arg(QDateTime::fromMSecsSinceEpoch(value, Qt::UTC).toString()));

    d->positionMs = d->lastSeekTimeMs = value;
    if (d->archiveReader)
    {
        d->archiveReader->jumpTo(msecToUsec(value), msecToUsec(value));
    }

    d->setLiveMode(value == kLivePosition);
    d->setMediaStatus(MediaStatus::Loading);
    d->clearCurrentFrame();
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

bool Player::checkReadyToPlay()
{
    Q_D(Player);

    if (d->archiveReader || d->initDataProvider())
        return true;

    d->log(lit("play() END: no data"));
    return false;
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

    if (!checkReadyToPlay())
        return;

    d->setState(State::Playing);
    d->setMediaStatus(MediaStatus::Loading);
    d->dataConsumer->setAudioEnabled(d->isAudioEnabled);

    d->lastVideoPtsMs.reset();
    d->at_hurryUp(); //< renew receiving frames

    d->log(lit("play() END"));
}

void Player::pause()
{
    Q_D(Player);
    d->log(lit("pause()"));
    d->setState(State::Paused);
    d->execTimer->stop(); //< stop next frame displaying
    if (d->dataConsumer)
        d->dataConsumer->setAudioEnabled(false);
}

void Player::preview()
{
    Q_D(Player);
    d->log(lit("preview()"));

    if (!checkReadyToPlay())
        return;

    d->setState(State::Previewing);
    d->dataConsumer->setAudioEnabled(false);
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
    d->clearCurrentFrame();
    d->updateCurrentResolution(QSize());

    d->setState(State::Stopped);
    if (d->mediaStatus != MediaStatus::NoVideoStreams) //< Preserve NoVideoStreams state.
        d->setMediaStatus(MediaStatus::NoMedia);
    d->log(lit("stop() END"));
}

void Player::setSource(const QUrl& url)
{
    Q_D(Player);

    const QUrl& newUrl = *ini().substitutePlayerUrl ? QUrl(ini().substitutePlayerUrl) : url;

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
        d->resource = commonModule()->resourcePool()->getResourceById(QnUuid(path));
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

bool Player::isAudioEnabled() const
{
    Q_D(const Player);
    return d->isAudioEnabled;
}

void Player::setAudioEnabled(bool value)
{
    Q_D(Player);
    if (d->isAudioEnabled != value)
    {
        d->isAudioEnabled = value;
        if (d->dataConsumer)
            d->dataConsumer->setAudioEnabled(value);
        emit audioEnabledChanged();
    }
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

    if (ini().forceIframesOnly && videoQuality == LowVideoQuality)
    {
        d->log(lit("setVideoQuality(%1): .ini forceIframesOnly is set => use value %2")
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

Player::VideoQuality Player::actualVideoQuality() const
{
    Q_D(const Player);
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
    Q_D(const Player);

    d->log(lit("availableVideoQualities() BEGIN"));

    QList<int> result;

    const auto transcodingCoded = d->archiveReader
        ? d->archiveReader->getTranscodingCodec()
        : QnArchiveStreamReader(d->resource).getTranscodingCodec();

    const auto camera = d->resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return result; //< Setting videoQuality for files is not supported.

    auto getQuality =
        [&camera,
            transcodingCoded,
            liveMode = d->liveMode,
            positionMs = d->positionMs,
            currentVideoDecoders = d->dataConsumer
                ? d->dataConsumer->currentVideoDecoders()
                : std::vector<AbstractVideoDecoder*>()](
                    int quality)
        {
            return media_player_quality_chooser::chooseVideoQuality(
                transcodingCoded,
                quality,
                liveMode,
                positionMs,
                camera,
                true,
                currentVideoDecoders);
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

                    resultQuality.quality = static_cast<Player::VideoQuality>(videoQuality);
                    customQualities.append(resultQuality);
                }
            }
        }
    }

    d->log(lit("availableVideoQualities() END"));

    if (customResolutionAvailable)
    {
        const auto uniqueCustomQualities = sortOutEqualQualities(customQualities);
        return result + uniqueCustomQualities;
    }

    return result;
}

bool Player::allowOverlay() const
{
    Q_D(const Player);
    return d->allowOverlay;
}

void Player::setAllowOverlay(bool allowOverlay)
{
    Q_D(Player);

    if (d->allowOverlay == allowOverlay)
    {
        d->log(lit("setAllowOverlay(%1): no change, ignoring").arg(allowOverlay));
        return;
    }
    d->log(lit("setAllowOverlay(%1)").arg(allowOverlay));
    d->allowOverlay = allowOverlay;
    emit allowOverlayChanged();
}

QSize Player::currentResolution() const
{
    Q_D(const Player);
    return d->currentResolution;
}

Player::TranscodingSupportStatus Player::transcodingStatus() const
{
    Q_D(const Player);

    const auto& camera = d->resource.dynamicCast<QnVirtualCameraResource>();
    if (!camera)
        return TranscodingDisabled;

    return media_player_quality_chooser::transcodingSupportStatus(
        camera, d->positionMs, d->liveMode);
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

PlayerStatistics Player::currentStatistics() const
{
    Q_D(const Player);

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
        result.bitrate += statistics->getBitrateMbps();
    }

    if (const auto codecContext = d->archiveReader->getCodecContext())
        result.codec = codecContext->getCodecName();
    result.isHwAccelerated = d->isHwAccelerated;
    return result;
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

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Player, VideoQuality)

} // namespace media
} // namespace nx
