#include "media_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtCore/QElapsedTimer>
#include <QtOpenGL/qgl.h>

#include <utils/common/delayed.h>
#include <ui/texture_size_helper.h>

#include "nx/streaming/archive_stream_reader.h"
#include "nx/streaming/rtsp_client_archive_delegate.h"
#include "core/resource_management/resource_pool.h"
#include "player_data_consumer.h"

namespace {
    const qint64 invalidTimestamp = -1;
	const qint64 kMaxFrameDuration = 1000 * 5; //< max allowed frame duration. If distance is higher, then frame discontinue
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
	std::unique_ptr<QnArchiveStreamReader> archiveReader; //< separate thread. This class performs network IO and gets compressed AV data
	std::unique_ptr<PlayerDataConsumer> dataConsumer;     //< separate thread. This class decodes compressed AV data
	QElapsedTimer ptsTimer;                               //< main AV timer for playback
	boost::optional<qint64> lastVideoPts;                 //< last video frame PTS
	qint64 ptsTimerBase;                                  //< timestamp when PTS timer was started
	bool hasAudio;
	QSharedPointer<QVideoFrame> videoFrameToRender;       //< decoded video which is awaiting to be rendered
protected:
	void at_gotVideoFrame();
	void presentNextFrame();
	qint64 getNextTimeToRender(qint64 pts);
	
protected:
	QUrl url;
	Player::State state;
	Player::MediaStatus mediaStatus;

	// -------------------- depracated ----------------------
	QElapsedTimer frameTimer;

	QAbstractVideoSurface *videoSurface;
	int position;
	qint64 timestamp;

	bool reconnectOnPlay;

	int maxTextureSize;

	PlayerPrivate(Player *parent);

	void setState(Player::State state);
	void setMediaStatus(Player::MediaStatus status);
	void updateMediaStatus();
};

PlayerPrivate::PlayerPrivate(Player *parent)
	: QObject(parent)
	, q_ptr(parent)
	, state(Player::State::Stopped)
	, videoSurface(0)
	, position(0)
	, timestamp(0)
	, reconnectOnPlay(false)
	, maxTextureSize(QnTextureSizeHelper::instance()->maxTextureSize())
	, ptsTimerBase(0)
	, hasAudio(false)
{
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

void PlayerPrivate::updateMediaStatus() 
{
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

	qint64 nextTimeToRender = getNextTimeToRender(videoFrameToRender->startTime());
	if (nextTimeToRender > ptsTimer.elapsed())
		executeDelayed([this]() { presentNextFrame(); }, nextTimeToRender - ptsTimer.elapsed());
	else
		presentNextFrame();
}

void PlayerPrivate::presentNextFrame()
{
	Q_Q(Player);

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
	
	emit q->positionChanged();

	videoFrameToRender.clear();
	at_gotVideoFrame(); //< calculate next time to render
}

qint64 PlayerPrivate::getNextTimeToRender(qint64 pts)
{
	Q_Q(Player);

	if (hasAudio)
	{
		// todo: not implemented yet
		return pts;
	}
	else 
	{
		/* Calculate time to present next frame */
		if (!lastVideoPts.is_initialized() || !qBetween(*lastVideoPts, pts, *lastVideoPts + kMaxFrameDuration))
		{
			// Reset timer If no previous frame or pts discontinue
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

Player::Player(QObject *parent)
	: QObject(parent)
	, d_ptr(new PlayerPrivate(this))
{
	Q_D(Player);

}

Player::~Player() 
{
	Q_D(Player);

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
	Q_D(const Player);
}

bool Player::reconnectOnPlay() const 
{
	Q_D(const Player);

	return d->reconnectOnPlay;
}

void Player::setReconnectOnPlay(bool reconnectOnPlay) 
{
	Q_D(Player);

	if (d->reconnectOnPlay == reconnectOnPlay)
		return;

	d->reconnectOnPlay = reconnectOnPlay;
	emit reconnectOnPlayChanged();
}

void Player::play()
{
	Q_D(Player);
	
	d->setState(State::Playing);
	d->setMediaStatus(MediaStatus::Loading);
	
	QnUuid id(d->url.path().mid(1));
	QnResourcePtr camera = qnResPool->getResourceById(id);
	if (!camera)
		return;

	d->archiveReader.reset(new QnArchiveStreamReader(camera));
	d->dataConsumer.reset(new PlayerDataConsumer());
	d->archiveReader->setArchiveDelegate(new QnRtspClientArchiveDelegate(d->archiveReader.get()));
	d->archiveReader->addDataProcessor(d->dataConsumer.get());
	connect(d->dataConsumer.get(), &PlayerDataConsumer::gotVideoFrame, d, &PlayerPrivate::at_gotVideoFrame);


	d->dataConsumer->start();
	d->archiveReader->start();

	d->archiveReader->open();
}

void Player::pause() 
{
	Q_D(Player);
}

void Player::stop()
{
	Q_D(Player);

	if (d->archiveReader && d->dataConsumer)
		d->archiveReader->removeDataProcessor(d->dataConsumer.get());

	d->dataConsumer.reset();
	d->archiveReader.reset();
}

void Player::setSource(const QUrl &url)
{
	Q_D(Player);

	if (url == d->url)
		return;

	stop();
	d->setState(State::Stopped);
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

}
}
