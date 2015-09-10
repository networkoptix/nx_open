#include "mjpeg_player.h"

#include <QtGui/QImage>
#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>

#include <utils/common/delayed.h>
#include "mjpeg_session.h"

#include <QElapsedTimer>

class QnMjpegPlayerPrivate : public QObject {
    Q_DECLARE_PUBLIC(QnMjpegPlayer)
    QnMjpegPlayer *q_ptr;

public:
    QnMjpegSession *session;
    QnMjpegPlayer::PlaybackState state;

    bool haveNewFrame;
    bool waitingForFrame;

    QAbstractVideoSurface *videoSurface;
    QElapsedTimer presentationTimer;
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
    , session(nullptr)
    , state(QnMjpegPlayer::Stopped)
    , haveNewFrame(false)
    , waitingForFrame(false)
    , videoSurface(0)
    , reconnectOnPlay(false)
{
    Q_Q(QnMjpegPlayer);
}

void QnMjpegPlayerPrivate::setState(QnMjpegPlayer::PlaybackState state) {
    if (state == this->state)
        return;

    this->state = state;

    Q_Q(QnMjpegPlayer);
    q->playbackStateChanged();
}

void QnMjpegPlayerPrivate::processFrame() {
    if (!haveNewFrame) {
        waitingForFrame = false;
        return;
    }

    if (state != QnMjpegPlayer::Playing)
        return;

    haveNewFrame = false;
    waitingForFrame = true;

    Q_Q(QnMjpegPlayer);

    int presentationTime;
    QByteArray imageData;

    if (!session->dequeueFrame(&imageData, &presentationTime)) {
        waitingForFrame = false;
        return;
    }

    position += presentationTime;

    QImage image;
    image.loadFromData(imageData);
    q->frameReady(image);

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

    if (presentationTimer.isValid())
        presentationTime -= presentationTimer.elapsed();

    presentationTimer.restart();

    q->positionChanged();

    if (presentationTime <= 0)
        processFrame();
    else
        executeDelayed([this](){ processFrame(); }, presentationTime);
}

void QnMjpegPlayerPrivate::at_session_frameEnqueued() {
    haveNewFrame = true;

    if (state != QnMjpegPlayer::Playing)
        return;

    if (!waitingForFrame)
        processFrame();
}


QnMjpegPlayer::QnMjpegPlayer(QObject *parent)
    : QObject(parent)
    , d_ptr(new QnMjpegPlayerPrivate(this))
{
    Q_D(QnMjpegPlayer);

    d->session = new QnMjpegSession(this);
    connect(d->session, &QnMjpegSession::frameEnqueued, d, &QnMjpegPlayerPrivate::at_session_frameEnqueued);
    connect(d->session, &QnMjpegSession::urlChanged, this, &QnMjpegPlayer::sourceChanged);
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
    d->session->play();

    if (sessionState == QnMjpegSession::Playing)
        d->processFrame();
}

void QnMjpegPlayer::pause() {
    Q_D(QnMjpegPlayer);

    if (d->state != Playing)
        return;

    d->setState(Paused);
    d->presentationTimer.invalidate();

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
    d->presentationTimer.invalidate();

    d->position = 0;
    emit positionChanged();
}

void QnMjpegPlayer::setSource(const QUrl &url) {
    Q_D(QnMjpegPlayer);

    d->session->setUrl(url);
}

void QnMjpegPlayer::setVideoSurface(QAbstractVideoSurface *videoSurface) {
    Q_D(QnMjpegPlayer);

    if (d->videoSurface == videoSurface)
        return;

    d->videoSurface = videoSurface;

    emit videoSurfaceChanged();;
}
