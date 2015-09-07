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
    QElapsedTimer displayTimer;
    qint64 lastFrameTime;
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
    , session(new QnMjpegSession(q_ptr))
    , state(QnMjpegPlayer::Stopped)
    , haveNewFrame(false)
    , waitingForFrame(false)
    , videoSurface(0)
    , lastFrameTime(0)
    , reconnectOnPlay(false)
{
    Q_Q(QnMjpegPlayer);

    connect(session, &QnMjpegSession::frameEnqueued, this, &QnMjpegPlayerPrivate::at_session_frameEnqueued);
    connect(session, &QnMjpegSession::urlChanged, q, &QnMjpegPlayer::sourceChanged);
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

    int displayTime;
    QByteArray imageData;

    if (!session->dequeueFrame(&imageData, &displayTime)) {
        waitingForFrame = false;
        return;
    }

    position += displayTime;

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

    if (displayTimer.isValid()) {
        displayTime -= displayTimer.elapsed() - lastFrameTime;
        lastFrameTime = displayTimer.elapsed();
    } else {
        displayTimer.start();
        lastFrameTime = 0;
    }

    q->positionChanged();

    if (displayTime <= 0)
        processFrame();
    else
        executeDelayed([this](){ processFrame(); }, displayTime);
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

    PlaybackState state = playbackState();

    if (state == Stopped) {
        d->position = 0;
        emit positionChanged();
    }

    if (state == Paused && d->reconnectOnPlay) {
        d->position = 0;
        d->session->stop();
    }

    d->setState(Playing);
    d->session->play();

    if (state == Paused)
        d->processFrame();
}

void QnMjpegPlayer::pause() {
    Q_D(QnMjpegPlayer);

    if (d->state != Playing)
        return;

    d->setState(Paused);
    d->displayTimer.invalidate();
}

void QnMjpegPlayer::stop() {
    Q_D(QnMjpegPlayer);

    d->session->stop();
    d->setState(Stopped);
    d->displayTimer.invalidate();
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
