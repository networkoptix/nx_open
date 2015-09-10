#include "mjpeg_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>

#include <utils/common/delayed.h>
#include "mjpeg_session.h"

class QnMjpegPlayerPrivate : public QObject {
    Q_DECLARE_PUBLIC(QnMjpegPlayer)
    QnMjpegPlayer *q_ptr;

public:
    QScopedPointer<QnMjpegSession> session;
    QThread *networkThread;
    QnMjpegPlayer::PlaybackState state;

    bool waitingForFrame;

    QAbstractVideoSurface *videoSurface;
    int position;

    bool reconnectOnPlay;

    QnMjpegPlayerPrivate(QnMjpegPlayer *parent);

    void setState(QnMjpegPlayer::PlaybackState state);
    void processFrame();
    void at_session_frameEnqueued();
};


QnMjpegPlayerPrivate::QnMjpegPlayerPrivate(QnMjpegPlayer *parent)
    : QObject(parent)
    , q_ptr(parent)
    , session(new QnMjpegSession())
    , state(QnMjpegPlayer::Stopped)
    , waitingForFrame(true)
    , videoSurface(0)
    , reconnectOnPlay(false)
{
}

void QnMjpegPlayerPrivate::setState(QnMjpegPlayer::PlaybackState state) {
    if (state == this->state)
        return;

    this->state = state;

    Q_Q(QnMjpegPlayer);
    q->playbackStateChanged();
}

void QnMjpegPlayerPrivate::processFrame() {
    if (!waitingForFrame)
        return;

    if (state != QnMjpegPlayer::Playing)
        return;

    int presentationTime;
    QImage image;

    if (!session->dequeueFrame(&image, &presentationTime))
        return;

    position += presentationTime;

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

    Q_Q(QnMjpegPlayer);

    q->positionChanged();

    if (presentationTime <= 0) {
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
    if (state != QnMjpegPlayer::Playing)
        return;

    if (waitingForFrame)
        processFrame();
}


QnMjpegPlayer::QnMjpegPlayer(QObject *parent)
    : QObject(parent)
    , d_ptr(new QnMjpegPlayerPrivate(this))
{
    Q_D(QnMjpegPlayer);

    connect(d->session.data(), &QnMjpegSession::frameEnqueued, d, &QnMjpegPlayerPrivate::at_session_frameEnqueued);
    connect(d->session.data(), &QnMjpegSession::urlChanged, this, &QnMjpegPlayer::sourceChanged);

    d->networkThread = new QThread(this);
    d->session->moveToThread(d->networkThread);
    d->networkThread->start();
}

QnMjpegPlayer::~QnMjpegPlayer() {
}

QnMjpegPlayer::PlaybackState QnMjpegPlayer::playbackState() const {
    Q_D(const QnMjpegPlayer);

    return d->state;
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

    if (d->state == Playing)
        return;

    QnMjpegSession::State sessionState = d->session->state();

    d->setState(Playing);
    d->session->start();

    if (sessionState == QnMjpegSession::Playing && d->waitingForFrame)
        d->processFrame();
}

void QnMjpegPlayer::pause() {
    Q_D(QnMjpegPlayer);

    if (d->state != Playing)
        return;

    d->setState(Paused);

    if (d->reconnectOnPlay) {
        d->session->stop();
        d->position = 0;
        emit positionChanged();
    }
}

void QnMjpegPlayer::stop() {
    Q_D(QnMjpegPlayer);

    d->session->stop();
    d->setState(Stopped);

    d->position = 0;
    emit positionChanged();
}

void QnMjpegPlayer::setSource(const QUrl &url) {
    Q_D(QnMjpegPlayer);

    if (url == d->session->url())
        return;

    d->session->setUrl(url);
    d->session->stop();
    d->session->start();

    emit sourceChanged();
}

void QnMjpegPlayer::setVideoSurface(QAbstractVideoSurface *videoSurface) {
    Q_D(QnMjpegPlayer);

    if (d->videoSurface == videoSurface)
        return;

    d->videoSurface = videoSurface;

    emit videoSurfaceChanged();;
}
