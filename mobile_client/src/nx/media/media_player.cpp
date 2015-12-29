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

namespace {
	static const qint64 kMaxFrameDuration = 1000 * 5; //< max allowed frame duration. If distance is higher, then frame discontinue
	static const qint64 kLivePosition = -1;
    static const int kMaxDelayForResyncMs = 200; //< resync playback timer if video frame late
    static const int kMaxLiveBufferMs = 200; //< maximum durartion for live buffer

    static qint64 msecToUsec(qint64 posMs)
    {
        return posMs == kLivePosition ? DATETIME_NOW : posMs * 1000ll;
    }

    static qint64 usecToMsec(qint64 posUsec)
    {
        return posUsec == DATETIME_NOW ? kLivePosition : posUsec / 1000ll;
    }
}

namespace nx {
namespace media {


class PlayerPrivate: public QObject
{
	Q_DECLARE_PUBLIC(Player)
	Player *q_ptr;

public:
    Player::State state;                                  //< Holds QT property value
    Player::MediaStatus mediaStatus;                      //< Holds QT property value
	bool hasAudio;                                        //< Either media has audio stream or not. Holds QT property value
    bool liveMode;                                        //< Either media on live or archive position. Holds QT property value
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
};

PlayerPrivate::PlayerPrivate(Player *parent):
    QObject(parent),
	q_ptr(parent),
	state(Player::State::Stopped),
    mediaStatus(Player::MediaStatus::NoMedia),
    hasAudio(false),
    liveMode(false),
	position(0),
    videoSurface(0),
	maxTextureSize(QnTextureSizeHelper::instance()->maxTextureSize()),
    ptsTimerBase(0),
    execTimer(new QTimer(this)),
    lastSeekTimeMs(AV_NOPTS_VALUE)
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
    setMediaStatus(Player::MediaStatus::Loaded);

    if (execTimer->isActive())
        return; //< we already have frame to render. Ignore next frame (will be processed later)

    if (state == Player::State::Stopped)
        return;

    if (!videoFrameToRender)
    {
        videoFrameToRender = dataConsumer->dequeueVideoFrame();
        if (!videoFrameToRender)
            return;
    }

    if (state == Player::State::Paused)
    {
        FrameMetadata metadata = FrameMetadata::deserialize(videoFrameToRender);
        if (!metadata.noDelay)
            return; //< display 'noDelay' frames only if player is paused
    }

    qint64 nextTimeToRender = getNextTimeToRender(videoFrameToRender);
    if (nextTimeToRender > ptsTimer.elapsed())
        execTimer->start(nextTimeToRender - ptsTimer.elapsed());
	else
		presentNextFrame();
}

void PlayerPrivate::presentNextFrame()
{
	if (!videoFrameToRender)
		return;

	/* update video surface's pixel format if need */
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
		videoSurface->present(*videoFrameToRender);

    setPosition(videoFrameToRender->startTime());

    auto metadata = FrameMetadata::deserialize(videoFrameToRender);
    setLiveMode(metadata.flags & QnAbstractMediaData::MediaFlags_LIVE);

	videoFrameToRender.clear();
	at_gotVideoFrame(); //< calculate next time to render
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
		if (!lastVideoPts.is_initialized() ||                                    //< first time
            !qBetween(*lastVideoPts, pts, *lastVideoPts + kMaxFrameDuration) ||  //< pts discontinue
            metadata.noDelay                                                 ||  //< jump occurred
            pts < lastSeekTimeMs                                             ||  //< 'coarse' frame. Frame time is less than required jump pos
            pts - ptsTimerBase < -kMaxDelayForResyncMs                       ||    //< resync because of video frame is late more than threshold
            (liveMode && usecToMsec(dataConsumer->lastMediaTimeUsec()) - pts > kMaxLiveBufferMs) //< live buffer overflow
            )
		{
			// Reset timer
			lastVideoPts = ptsTimerBase = pts;
			ptsTimer.restart();
			return 0ll;
		}
		else {
			lastVideoPts = pts;
			return pts - ptsTimerBase;
		}
	}
}

bool PlayerPrivate::initDataProvider()
{
    QnUuid id(url.path().mid(1));
    QnResourcePtr camera = qnResPool->getResourceById(id);
    if (!camera) {
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
    d->at_gotVideoFrame(); //< renew receiving frames
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
