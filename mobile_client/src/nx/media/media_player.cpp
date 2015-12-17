#if 0

#include "media_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtCore/QElapsedTimer>
#include <QtOpenGL/qgl.h>

#include <utils/common/delayed.h>
#include <ui/texture_size_helper.h>
#include "mjpeg_session.h"

namespace {
    const qint64 invalidTimestamp = -1;
}

class QnMjpegPlayerPrivate : public QObject {
    Q_DECLARE_PUBLIC(QnMjpegPlayer)
    QnMjpegPlayer *q_ptr;

public:
    QnMjpegSession *session;
    QMediaPlayer::State state;
    QMediaPlayer::MediaStatus mediaStatus;

    bool waitingForFrame;
    QElapsedTimer frameTimer;
    int framePresentationTime;

    QAbstractVideoSurface *videoSurface;
    int position;
    qint64 timestamp;

    bool reconnectOnPlay;

    int maxTextureSize;

    QnMjpegPlayerPrivate(QnMjpegPlayer *parent);

    void setState(QMediaPlayer::State state);
    void setMediaStatus(QMediaPlayer::MediaStatus status);
    void updateMediaStatus();
    void processFrame();
    void at_session_frameEnqueued();
};


QnMjpegPlayerPrivate::QnMjpegPlayerPrivate(QnMjpegPlayer *parent)
    : QObject(parent)
    , q_ptr(parent)
    , session(new QnMjpegSession())
    , state(QMediaPlayer::StoppedState)
    , waitingForFrame(true)
    , framePresentationTime(0)
    , videoSurface(0)
    , position(0)
    , timestamp(0)
    , reconnectOnPlay(false)
    , maxTextureSize(QnTextureSizeHelper::instance()->maxTextureSize())
{
}

void QnMjpegPlayerPrivate::setState(QMediaPlayer::State state) {
    if (state == this->state)
        return;

    this->state = state;

    Q_Q(QnMjpegPlayer);
    emit q->playbackStateChanged();
}

void QnMjpegPlayerPrivate::setMediaStatus(QMediaPlayer::MediaStatus status) {
    if (mediaStatus == status)
        return;

    mediaStatus = status;

    Q_Q(QnMjpegPlayer);
    emit q->mediaStatusChanged();
}

void QnMjpegPlayerPrivate::updateMediaStatus() {
    QMediaPlayer::MediaStatus status = QMediaPlayer::UnknownMediaStatus;

    switch (session->state()) {
    case QnMjpegSession::Stopped:
        status = QMediaPlayer::UnknownMediaStatus;
        break;
    case QnMjpegSession::Connecting:
        status = QMediaPlayer::LoadingMedia;
        break;
    case QnMjpegSession::Disconnecting:
        status = QMediaPlayer::EndOfMedia;
        break;
    case QnMjpegSession::Playing:
        status = (position == 0) ? QMediaPlayer::BufferingMedia : QMediaPlayer::BufferedMedia;
        break;
    }

    setMediaStatus(status);
}

void QnMjpegPlayerPrivate::processFrame() {
    if (!waitingForFrame)
        return;

    if (state != QMediaPlayer::PlayingState)
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

    Q_Q(QnMjpegPlayer);
    emit q->positionChanged();

    framePresentationTime = qMax(0, presentationTime);

    if (framePresentationTime == 0) {
        processFrame();
    } else {
        waitingForFrame = false;
        executeDelayed([this](){
            waitingForFrame = true;
            processFrame();
        }, presentationTime);
    }
}

void QnMjpegPlayerPrivate::at_session_frameEnqueued() {
    if (state != QMediaPlayer::PlayingState)
        return;

    if (waitingForFrame)
        processFrame();
}


QnMjpegPlayer::QnMjpegPlayer(QObject *parent)
    : QObject(parent)
    , d_ptr(new QnMjpegPlayerPrivate(this))
{
    Q_D(QnMjpegPlayer);

    QThread *networkThread;
    networkThread = new QThread();
    networkThread->setObjectName(lit("MJPEG Session Thread"));
    d->session->moveToThread(networkThread);
    networkThread->start();

    connect(d->session, &QnMjpegSession::frameEnqueued,             d,      &QnMjpegPlayerPrivate::at_session_frameEnqueued);
    connect(d->session, &QnMjpegSession::urlChanged,                this,   &QnMjpegPlayer::sourceChanged);
    connect(d->session, &QnMjpegSession::finished,                  this,   &QnMjpegPlayer::playbackFinished);
    connect(d->session, &QnMjpegSession::finalTimestampChanged,     this,   &QnMjpegPlayer::finalTimestampChanged);
    connect(d->session, &QnMjpegSession::stateChanged,              d,      &QnMjpegPlayerPrivate::updateMediaStatus);
    connect(this,       &QnMjpegPlayer::positionChanged,            d,      &QnMjpegPlayerPrivate::updateMediaStatus);
    connect(this,       &QnMjpegPlayer::positionChanged,            this,   &QnMjpegPlayer::timestampChanged);
}

QnMjpegPlayer::~QnMjpegPlayer() {
    Q_D(QnMjpegPlayer);

    QnMjpegSession *session = d->session;
    connect(session, &QnMjpegSession::stateChanged, session, [session]() {
        if (session->state() == QnMjpegSession::Stopped) {
            session->deleteLater();
            QMetaObject::invokeMethod(session->thread(), "quit", Qt::QueuedConnection);
        }
    });

    stop();
}

QMediaPlayer::State QnMjpegPlayer::playbackState() const {
    Q_D(const QnMjpegPlayer);

    return d->state;
}

QMediaPlayer::MediaStatus QnMjpegPlayer::mediaStatus() const {
    Q_D(const QnMjpegPlayer);
    return d->mediaStatus;
}

QUrl QnMjpegPlayer::source() const {
    Q_D(const QnMjpegPlayer);

    return d->session->url();
}

QAbstractVideoSurface *QnMjpegPlayer::videoSurface() const {
    Q_D(const QnMjpegPlayer);

    return d->videoSurface;
}

int QnMjpegPlayer::position() const {
    Q_D(const QnMjpegPlayer);

    return d->position;
}

qint64 QnMjpegPlayer::timestamp() const {
    Q_D(const QnMjpegPlayer);

    return d->timestamp;
}

qint64 QnMjpegPlayer::finalTimestamp() const {
    Q_D(const QnMjpegPlayer);
    return d->session->finalTimestampMs();
}

void QnMjpegPlayer::setFinalTimestamp(qint64 finalTimestamp) {
    Q_D(QnMjpegPlayer);
    d->session->setFinalTimestampMs(finalTimestamp);
}

bool QnMjpegPlayer::reconnectOnPlay() const {
    Q_D(const QnMjpegPlayer);

    return d->reconnectOnPlay;
}

void QnMjpegPlayer::setReconnectOnPlay(bool reconnectOnPlay) {
    Q_D(QnMjpegPlayer);

    if (d->reconnectOnPlay == reconnectOnPlay)
        return;

    d->reconnectOnPlay = reconnectOnPlay;
    emit reconnectOnPlayChanged();
}

void QnMjpegPlayer::play() {
    Q_D(QnMjpegPlayer);

    if (d->state == QMediaPlayer::PlayingState)
        return;

    QnMjpegSession::State sessionState = d->session->state();

    d->frameTimer.invalidate();
    d->setState(QMediaPlayer::PlayingState);
    d->session->start();

    if (sessionState == QnMjpegSession::Playing && d->waitingForFrame)
        d->processFrame();
}

void QnMjpegPlayer::pause() {
    Q_D(QnMjpegPlayer);

    if (d->state != QMediaPlayer::PlayingState)
        return;

    d->setState(QMediaPlayer::PausedState);

    if (d->reconnectOnPlay) {
        d->session->stop();

        if (d->position != 0) {
            d->position = 0;
            d->timestamp = invalidTimestamp;
            emit positionChanged();
        }
    }
}

void QnMjpegPlayer::stop() {
    Q_D(QnMjpegPlayer);

    d->session->stop();
    d->setState(QMediaPlayer::StoppedState);

    if (d->position != 0) {
        d->position = 0;
        d->timestamp = invalidTimestamp;
        emit positionChanged();
    }
}

void QnMjpegPlayer::setSource(const QUrl &url) {
    Q_D(QnMjpegPlayer);

    if (url == d->session->url())
        return;

    d->waitingForFrame = true;
    d->session->setUrl(url);
    d->session->stop();

    if (d->position != 0) {
        d->position = 0;
        d->timestamp = invalidTimestamp;
        emit positionChanged();
    }

    d->session->start();
}

void QnMjpegPlayer::setVideoSurface(QAbstractVideoSurface *videoSurface) {
    Q_D(QnMjpegPlayer);

    if (d->videoSurface == videoSurface)
        return;

    d->videoSurface = videoSurface;

    emit videoSurfaceChanged();
}

#endif
