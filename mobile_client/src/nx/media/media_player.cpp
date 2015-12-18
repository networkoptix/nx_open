#include "media_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtCore/QElapsedTimer>
#include <QtOpenGL/qgl.h>

#include <utils/common/delayed.h>
#include <ui/texture_size_helper.h>
#include <utils/mjpeg/mjpeg_session.h>

#include "nx/streaming/archive_stream_reader.h"
#include "nx/streaming/rtsp_client_archive_delegate.h"
#include "core/resource_management/resource_pool.h"

namespace {
    const qint64 invalidTimestamp = -1;
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
	std::unique_ptr<QnArchiveStreamReader> archiveReader;
protected:
	
	// -------------------- depracated ----------------------

	QnMjpegSession *session;
	Player::State state;
	Player::MediaStatus mediaStatus;

	bool waitingForFrame;
	QElapsedTimer frameTimer;
	int framePresentationTime;

	QAbstractVideoSurface *videoSurface;
	int position;
	qint64 timestamp;

	bool reconnectOnPlay;

	int maxTextureSize;

	PlayerPrivate(Player *parent);

	void setState(Player::State state);
	void setMediaStatus(Player::MediaStatus status);
	void updateMediaStatus();
	void processFrame();
	void at_session_frameEnqueued();
};


PlayerPrivate::PlayerPrivate(Player *parent)
	: QObject(parent)
	, q_ptr(parent)
	, session(new QnMjpegSession())
	, state(Player::State::Stopped)
	, waitingForFrame(true)
	, framePresentationTime(0)
	, videoSurface(0)
	, position(0)
	, timestamp(0)
	, reconnectOnPlay(false)
	, maxTextureSize(QnTextureSizeHelper::instance()->maxTextureSize())
{
}

void PlayerPrivate::setState(Player::State state) {
	if (state == this->state)
		return;

	this->state = state;

	Q_Q(Player);
	emit q->playbackStateChanged();
}

void PlayerPrivate::setMediaStatus(Player::MediaStatus status) {
	if (mediaStatus == status)
		return;

	mediaStatus = status;

	Q_Q(Player);
	emit q->mediaStatusChanged();
}

void PlayerPrivate::updateMediaStatus() {
	Player::MediaStatus status = Player::MediaStatus::Unknown;

	switch (session->state()) {
	case QnMjpegSession::Stopped:
		status = Player::MediaStatus::Unknown;
		break;
	case QnMjpegSession::Connecting:
		status = Player::MediaStatus::Loading;
		break;
	case QnMjpegSession::Disconnecting:
		status = Player::MediaStatus::EndOfMedia;
		break;
	case QnMjpegSession::Playing:
		status = (position == 0) ? Player::MediaStatus::Buffering : Player::MediaStatus::Buffered;
		break;
	}

	setMediaStatus(status);
}

void PlayerPrivate::processFrame() {
	if (!waitingForFrame)
		return;

	if (state != Player::State::Playing)
		return;

	int presentationTime;
	QImage image;

	if (!session->dequeueFrame(&image, &timestamp, &presentationTime))
		return;

	position += presentationTime;

	if (frameTimer.isValid())
		presentationTime -= qMax(0, static_cast<int>(frameTimer.elapsed()) - framePresentationTime);

	if (image.width() > maxTextureSize || image.height() > maxTextureSize)
		image = image.scaled(maxTextureSize, maxTextureSize, Qt::KeepAspectRatio);

	QVideoFrame frame(image);
	if (videoSurface) {
		if (videoSurface->isActive() && videoSurface->surfaceFormat().pixelFormat() != frame.pixelFormat())
			videoSurface->stop();

		if (!videoSurface->isActive()) {
			QVideoSurfaceFormat format(frame.size(), frame.pixelFormat(), frame.handleType());
			videoSurface->start(format);
		}

		if (videoSurface->isActive())
			videoSurface->present(frame);
	}

	frameTimer.restart();

	Q_Q(Player);
	emit q->positionChanged();

	framePresentationTime = qMax(0, presentationTime);

	if (framePresentationTime == 0) {
		processFrame();
	}
	else {
		waitingForFrame = false;
		executeDelayed([this](){
			waitingForFrame = true;
			processFrame();
		}, presentationTime);
	}
}

void PlayerPrivate::at_session_frameEnqueued() {
	if (state != Player::State::Playing)
		return;

	if (waitingForFrame)
		processFrame();
}


Player::Player(QObject *parent)
	: QObject(parent)
	, d_ptr(new PlayerPrivate(this))
{
	Q_D(Player);

	QThread *networkThread;
	networkThread = new QThread();
	networkThread->setObjectName(lit("MJPEG Session Thread"));
	d->session->moveToThread(networkThread);
	networkThread->start();

	connect(d->session, &QnMjpegSession::frameEnqueued, d, &PlayerPrivate::at_session_frameEnqueued);
	connect(d->session, &QnMjpegSession::urlChanged, this, &Player::sourceChanged);
	connect(d->session, &QnMjpegSession::finished, this, &Player::playbackFinished);
	connect(d->session, &QnMjpegSession::stateChanged, d, &PlayerPrivate::updateMediaStatus);
	connect(this, &Player::positionChanged, d, &PlayerPrivate::updateMediaStatus);
}

Player::~Player() 
{
	Q_D(Player);

	QnMjpegSession *session = d->session;
	connect(session, &QnMjpegSession::stateChanged, session, [session]() {
		if (session->state() == QnMjpegSession::Stopped) {
			session->deleteLater();
			QMetaObject::invokeMethod(session->thread(), "quit", Qt::QueuedConnection);
		}
	});

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

	return d->session->url();
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
	d->archiveReader->open();
}

void Player::pause() 
{
	Q_D(Player);
}

void Player::stop()
{
	Q_D(Player);
}

void Player::setSource(const QUrl &url)
{
	Q_D(Player);

	if (url == d->session->url())
		return;
	
	d->archiveReader.reset();
	d->setState(State::Stopped);

	QnUuid id(url.path().mid(1));
	QnResourcePtr camera = qnResPool->getResourceById(id);
	if (!camera)
		return;

	d->archiveReader.reset(new QnArchiveStreamReader(camera));
	d->archiveReader->setArchiveDelegate(new QnRtspClientArchiveDelegate(d->archiveReader.get()));
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
