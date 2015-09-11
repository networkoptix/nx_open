#include "mjpeg_player.h"

#include <QtMultimedia/QAbstractVideoSurface>
#include <QtMultimedia/QVideoSurfaceFormat>
#include <QtCore/QElapsedTimer>

#include <utils/common/delayed.h>
#include "mjpeg_session.h"

class QnMjpegPlayerPrivate : public QObject {
    Q_DECLARE_PUBLIC(QnMjpegPlayer)
    QnMjpegPlayer *q_ptr;

public:
    QnMjpegSession *session;
    QnMjpegPlayer::PlaybackState state;

    bool waitingForFrame;
    QElapsedTimer frameTimer;
    int framePresentationTime;

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
    , framePresentationTime(0)
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

    if (frameTimer.isValid())
        presentationTime -= qMax(0, static_cast<int>(frameTimer.elapsed()) - framePresentationTime);

    qDebug() << presentationTime;

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
    q->positionChanged();

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

    connect(d->session, &QnMjpegSession::frameEnqueued, d, &QnMjpegPlayerPrivate::at_session_frameEnqueued);
    connect(d->session, &QnMjpegSession::urlChanged, this, &QnMjpegPlayer::sourceChanged);

    QThread *networkThread;
    networkThread = new QThread();
    networkThread->setObjectName(lit("MJPEG Session Thread"));
    d->session->moveToThread(networkThread);
    networkThread->start();
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

    d->frameTimer.invalidate();
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

    d->waitingForFrame = true;
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
