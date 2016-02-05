#include "media_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtCore/QElapsedTimer>
#include <QtOpenGL/qgl.h>

#include <utils/common/delayed.h>
#include "core/resource_management/resource_pool.h"
#include <ui/texture_size_helper.h>

#include "nx/streaming/archive_stream_reader.h"
#include "nx/streaming/rtsp_client_archive_delegate.h"

#include "player_data_consumer.h"
#include "frame_metadata.h"

namespace nx {
namespace media {
    
namespace {
    static const qint64 kMaxFrameDuration = 1000 * 5; //< max allowed frame duration. If distance is higher, then frame discontinue
    static const qint64 kLivePosition = -1;
    static const int kMaxDelayForResyncMs = 500; //< resync playback timer if video frame too late
    static const int kInitialLiveBufferMs = 200; //< initial duration for live buffer
    static const int kMaxLiveBufferMs = 1200;    //< maximum duration for live buffer. Live buffer can be extended dynamically
    static const qreal kLiveBufferStep = 2.0;    //< increate live buffer at N times if overflow/underflow issues
    static const int kMaxCounterForWrongLiveBuffer = 2; //< Max allowed amount of underflow/overflow issues in Live mode before extending live buffer

    static qint64 msecToUsec(qint64 posMs)
    {
        return posMs == kLivePosition ? DATETIME_NOW : posMs * 1000ll;
    }

    static qint64 usecToMsec(qint64 posUsec)
    {
        return posUsec == DATETIME_NOW ? kLivePosition : posUsec / 1000ll;
    }
}

class PlayerPrivate: public QObject
{
	Q_DECLARE_PUBLIC(Player)
	Player *q_ptr;

public:
    Player::State state;                                  //< Holds QT property value
    Player::MediaStatus mediaStatus;                      //< Holds QT property value
    bool hasAudio;                                        //< Either media has audio stream or not. Holds QT property value
    bool liveMode;                                        //< Either media is on live or archive position. Holds QT property value
    qint64 position;                                      //< UTC Playback position at msec. Holds QT property value
    QAbstractVideoSurface* videoSurface;                  //< Video surface to render. Holds QT property value
    QUrl url;                                             //< media URL to play. Holds QT property value
    int maxTextureSize;

    QElapsedTimer ptsTimer;                               //< main AV timer for playback
    boost::optional<qint64> lastVideoPts;                 //< last video frame PTS
    qint64 ptsTimerBase;                                  //< timestamp when the PTS timer was started
    QnVideoFramePtr videoFrameToRender;                   //< decoded video which is awaiting to be rendered
    std::unique_ptr<QnArchiveStreamReader> archiveReader; //< separate thread. This class performs network IO and gets compressed AV data
    std::unique_ptr<PlayerDataConsumer> dataConsumer;     //< separate thread. This class decodes compressed AV data
    QTimer* execTimer;                                    //< timer for delayed call 'presentFrame'
    qint64 lastSeekTimeMs;                                //< last seek position. UTC time in msec
    int liveBufferMs;                                     //< current duration of live buffer in range [kInitialLiveBufferMs.. kMaxLiveBufferMs]

    enum class BufferState
    {
        NoIssue,
        Underflow,
        Overflow
    };

    BufferState liveBufferState;        //< live buffer state for the last frame
    int underflowCounter;               //< live buffer underflow counter
    int overflowCounter;                //< live buffer overflow counter
private:
    PlayerPrivate(Player *parent);

    void at_hurryUp();
	void at_gotVideoFrame();
	
    void presentNextFrame();
    qint64 getNextTimeToRender(const QnVideoFramePtr& frame);
    bool initDataProvider();

	void setState(Player::State state);
	void setMediaStatus(Player::MediaStatus status);
    void setLiveMode(bool value);
    void setPosition(qint64 value);

    void resetLiveBufferState();
    void updateLiveBufferState(BufferState value);

    QnVideoFramePtr scaleFrame(const QnVideoFramePtr& videoFrame);
};

PlayerPrivate::PlayerPrivate(Player *parent):
    QObject(parent),
	q_ptr(parent),
	state(Player::State::Stopped),
    mediaStatus(Player::MediaStatus::NoMedia),
    hasAudio(false),
    liveMode(true),
	position(0),
    videoSurface(0),
	maxTextureSize(QnTextureSizeHelper::instance()->maxTextureSize()),
    ptsTimerBase(0),
    execTimer(new QTimer(this)),
    lastSeekTimeMs(AV_NOPTS_VALUE),
    liveBufferMs(kInitialLiveBufferMs),
    liveBufferState(BufferState::NoIssue),
    underflowCounter(0),
    overflowCounter(0)
{
    connect(execTimer, &QTimer::timeout, this, &PlayerPrivate::presentNextFrame);
    execTimer->setSingleShot(true);
}

void PlayerPrivate::setState(Player::State state) {
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
    if (position == value)
        return;

    position = value;
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
        return; //< we already have frame to render. Ignore next frame (will be processed later)

    videoFrameToRender = dataConsumer->dequeueVideoFrame();
    if (!videoFrameToRender)
        return;

    if (state == Player::State::Paused)
    {
        FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);
        if (!metadata.noDelay)
            return; //< display regular frames only if player is playing
    }

    qint64 nextTimeToRender = getNextTimeToRender(videoFrameToRender);
    if (nextTimeToRender > ptsTimer.elapsed())
        execTimer->start(nextTimeToRender - ptsTimer.elapsed());
	else
		presentNextFrame();
}

QnVideoFramePtr PlayerPrivate::scaleFrame(const QnVideoFramePtr& videoFrame)
{
    if (videoFrame->width() <= maxTextureSize && videoFrame->height() <= maxTextureSize)
        return videoFrame; //< scale isn't required

    QImage::Format imageFormat = QVideoFrame::imageFormatFromPixelFormat(videoFrame->pixelFormat());
    videoFrame->map(QAbstractVideoBuffer::ReadOnly);
    QImage img(
        videoFrame->bits(),
        videoFrame->width(),
        videoFrame->height(),
        videoFrame->bytesPerLine(),
        imageFormat);
    QVideoFrame* scaledFrame = new QVideoFrame(img.scaled(maxTextureSize, maxTextureSize, Qt::KeepAspectRatio));
    videoFrame->unmap();

    scaledFrame->setStartTime(videoFrame->startTime());
    return QnVideoFramePtr(scaledFrame);
}

void PlayerPrivate::presentNextFrame()
{
	if (!videoFrameToRender)
		return;

    setMediaStatus(Player::MediaStatus::Loaded);

    /* update video surface's pixel format if needed */
	if (videoSurface)
	{
		if (videoSurface->isActive() && videoSurface->surfaceFormat().pixelFormat() != videoFrameToRender->pixelFormat())
			videoSurface->stop();

		if (!videoSurface->isActive()) {
			QVideoSurfaceFormat format(videoFrameToRender->size(), videoFrameToRender->pixelFormat(), videoFrameToRender->handleType());
			videoSurface->start(format);
		}
	}

    if (videoSurface && videoSurface->isActive())
        videoSurface->present(*scaleFrame(videoFrameToRender));

    setPosition(videoFrameToRender->startTime());

    auto metadata = FrameMetadata::deserialize(videoFrameToRender);
    setLiveMode(metadata.flags & QnAbstractMediaData::MediaFlags_LIVE);

	videoFrameToRender.clear();
    QTimer::singleShot(0, this, &PlayerPrivate::at_gotVideoFrame); //< calculate next time to render
}

void PlayerPrivate::resetLiveBufferState()
{
    overflowCounter = underflowCounter = 0;
    liveBufferState = BufferState::NoIssue;
    liveBufferMs = kInitialLiveBufferMs;
}

void PlayerPrivate::updateLiveBufferState(BufferState value)
{
    if (liveBufferState == value)
        return; //< do not take into account same state several times in a row
    liveBufferState = value;

    if (liveBufferState == BufferState::Overflow)
        overflowCounter++;
    else if (liveBufferState == BufferState::Underflow)
        underflowCounter++;
    else
        return;

    if (underflowCounter + overflowCounter >= kMaxCounterForWrongLiveBuffer)
        liveBufferMs = qMin(liveBufferMs * kLiveBufferStep, kMaxLiveBufferMs); //< too much underflow/overflow issues. extend live buffer
}

qint64 PlayerPrivate::getNextTimeToRender(const QnVideoFramePtr& frame)
{
    const qint64 pts = frame->startTime();
	if (hasAudio)
	{
		// todo: audio isn't implemented yet
		return pts;
	}
	else 
	{
        FrameMetadata metadata = FrameMetadata::deserialize(frame);
        
        // Calculate time to present next frame
        qint64 mediaQueueLen = usecToMsec(dataConsumer->queueVideoDurationUsec());
        const int frameDelayMs = pts - ptsTimerBase - ptsTimer.elapsed();
        bool liveBufferUnderflow = liveMode && lastVideoPts.is_initialized() && mediaQueueLen == 0 && frameDelayMs < 0;
        bool liveBufferOverflow = liveMode && mediaQueueLen > liveBufferMs;

        if (liveMode)
        {
            if (metadata.noDelay)
            {
                resetLiveBufferState(); //< reset live statstics because of seek or FCZ frames
            }
            else {
                auto value = liveBufferUnderflow ? BufferState::Underflow : liveBufferOverflow ? BufferState::Overflow : BufferState::NoIssue;
                updateLiveBufferState(value);
            }
        }

        if (!lastVideoPts.is_initialized() ||                                    //< first time
            !qBetween(*lastVideoPts, pts, *lastVideoPts + kMaxFrameDuration) ||  //< pts discontinue
            metadata.noDelay                                                 ||  //< jump occurred
            pts < lastSeekTimeMs                                             ||  //< 'coarse' frame. Frame time is less than required jump pos
            frameDelayMs < -kMaxDelayForResyncMs                             ||  //< resync because of video frame is late more than threshold
            liveBufferUnderflow                                              ||
            liveBufferOverflow                                                   //< live buffer overflow
            )
		{
            if (liveBufferOverflow)
                qDebug() << "reset because live overflow. len" << mediaQueueLen;

			// Reset timer
			lastVideoPts = ptsTimerBase = pts;
			ptsTimer.restart();
			return 0ll;
		}
		else 
        {
			lastVideoPts = pts;
			return pts - ptsTimerBase;
		}
    }
}

bool PlayerPrivate::initDataProvider()
{
    QnUuid id(url.path().mid(1));
    QnResourcePtr camera = qnResPool->getResourceById(id);
    if (!camera) 
    {
        setMediaStatus(Player::MediaStatus::NoMedia);
        return false;
    }

    archiveReader.reset(new QnArchiveStreamReader(camera));
    dataConsumer.reset(new PlayerDataConsumer(archiveReader));

    archiveReader->setArchiveDelegate(new QnRtspClientArchiveDelegate(archiveReader.get()));
    archiveReader->addDataProcessor(dataConsumer.get());
    connect(dataConsumer.get(), &PlayerDataConsumer::gotVideoFrame, this, &PlayerPrivate::at_gotVideoFrame);
    connect(dataConsumer.get(), &PlayerDataConsumer::hurryUp, this, &PlayerPrivate::at_hurryUp);
    connect(dataConsumer.get(), &PlayerDataConsumer::onEOF, this, [this]() { setPosition(kLivePosition);  });

    if (position != kLivePosition)
        archiveReader->jumpTo(msecToUsec(position), msecToUsec(position)); //< second arg means precise seek

    dataConsumer->start();
    archiveReader->start();

    return true;
}


// ----------------------- Player -----------------------

Player::Player(QObject *parent):
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

QUrl Player::source() const {
	Q_D(const Player);

	return d->url;
}

QAbstractVideoSurface *Player::videoSurface() const {
	Q_D(const Player);

	return d->videoSurface;
}

qint64 Player::position() const 
{
	Q_D(const Player);

	return d->position;
}

void Player::setPosition(qint64 value)
{
	Q_D(Player);
    d->lastSeekTimeMs = value;

    if (d->archiveReader)
        d->archiveReader->jumpTo(msecToUsec(value), 0);
    else
        d->position = value;

    d->at_hurryUp(); //< renew receiving frames
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

void Player::setSource(const QUrl &url)
{
	Q_D(Player);

	if (url == d->url)
		return;

	stop();
	d->url = url;
}

void Player::setVideoSurface(QAbstractVideoSurface *videoSurface)
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


} // namespace media
} // namespace nx
