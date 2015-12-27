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
    const qint64 invalidTimestamp = -1;
	const qint64 kMaxFrameDuration = 1000 * 5; //< max allowed frame duration. If distance is higher, then frame discontinue
	const qint64 kLivePosition = -1;

    qint64 msecToUsec(qint64 posMs)
    {
        return posMs == kLivePosition ? DATETIME_NOW : posMs * 1000ll;
    }
}

namespace nx
{
namespace media
{

class PlayerPrivate: public QObject
{
	Q_DECLARE_PUBLIC(Player)
	Player *q_ptr;

public:
    Player::State state;
    Player::MediaStatus mediaStatus;
    
	QElapsedTimer ptsTimer;                               //< main AV timer for playback
	boost::optional<qint64> lastVideoPts;                 //< last video frame PTS
	qint64 ptsTimerBase;                                  //< timestamp when PTS timer was started
	bool hasAudio;
    bool liveMode;
    qint64 position;                                      //< UTC Playback position at msec 
    QAbstractVideoSurface* videoSurface;
    int maxTextureSize;

    QnVideoFramePtr videoFrameToRender;                   //< decoded video which is awaiting to be rendered
    std::unique_ptr<QnArchiveStreamReader> archiveReader; //< separate thread. This class performs network IO and gets compressed AV data
    std::unique_ptr<PlayerDataConsumer> dataConsumer;     //< separate thread. This class decodes compressed AV data
    QUrl url;                                             //< media URL to play
    QTimer* execTimer;                                    //< timer for delayed call 'presentFrame'
protected:
    void at_hurryUp();
	void at_gotVideoFrame();
	void presentNextFrame();
    qint64 getNextTimeToRender(const QnVideoFramePtr& frame);

	PlayerPrivate(Player *parent);

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
    ptsTimerBase(0),
    hasAudio(false),
    liveMode(false),
	position(0),
    videoSurface(0),
	maxTextureSize(QnTextureSizeHelper::instance()->maxTextureSize()),
    execTimer(new QTimer(this))
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

    if (videoFrameToRender)
        return; //< we already have fame to render. Ignore next frame (will be processed later)

    if (state != Player::State::Playing)
        return;

    videoFrameToRender = dataConsumer->dequeueVideoFrame();
    if (!videoFrameToRender)
        return;

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
		// todo: not implemented yet
		return pts;
	}
	else 
	{
        FrameMetadata metadata = FrameMetadata::deserialize(frame);
		/* Calculate time to present next frame */
		if (!lastVideoPts.is_initialized() ||                                    //< first time
            !qBetween(*lastVideoPts, pts, *lastVideoPts + kMaxFrameDuration) ||  //< pts discontinue
            metadata.noDelay)                                                    //< jump occurred
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

// ----------------------- Player -----------------------

Player::Player(QObject *parent)
	: QObject(parent)
	, d_ptr(new PlayerPrivate(this))
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

	d->setState(State::Playing);
	d->setMediaStatus(MediaStatus::Loading);

	QnUuid id(d->url.path().mid(1));
	QnResourcePtr camera = qnResPool->getResourceById(id);
	if (!camera) {
		d->setMediaStatus(MediaStatus::NoMedia);
		return;
	}

	d->archiveReader.reset(new QnArchiveStreamReader(camera));
    d->dataConsumer.reset(new PlayerDataConsumer(d->archiveReader));
	d->archiveReader->setArchiveDelegate(new QnRtspClientArchiveDelegate(d->archiveReader.get()));
	d->archiveReader->addDataProcessor(d->dataConsumer.get());
	connect(d->dataConsumer.get(), &PlayerDataConsumer::gotVideoFrame, d, &PlayerPrivate::at_gotVideoFrame);
    connect(d->dataConsumer.get(), &PlayerDataConsumer::hurryUp, d, &PlayerPrivate::at_hurryUp);
    connect(d->dataConsumer.get(), &PlayerDataConsumer::onEOF, d, [this]() { setPosition(kLivePosition);  });

	if (d->position != kLivePosition) 
        d->archiveReader->jumpTo(msecToUsec(d->position), msecToUsec(d->position)); //< second arg means precise seek

	d->dataConsumer->start();
	d->archiveReader->start();

	d->archiveReader->open();
}

void Player::pause() 
{
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


}
}
