#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtMultimedia/QMediaPlayer>

class QAbstractVideoSurface;

class QnMjpegPlayerPrivate;
class QnMjpegPlayer : public QObject {
    Q_OBJECT

    Q_ENUMS(PlaybackState)
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)
    Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
    Q_PROPERTY(QAbstractVideoSurface * videoSurface READ videoSurface WRITE setVideoSurface NOTIFY videoSurfaceChanged)
    Q_PROPERTY(int position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 timestamp READ timestamp NOTIFY timestampChanged)
    Q_PROPERTY(qint64 finalTimestamp READ finalTimestamp WRITE setFinalTimestamp NOTIFY finalTimestampChanged)
    Q_PROPERTY(bool reconnectOnPlay READ reconnectOnPlay WRITE setReconnectOnPlay NOTIFY reconnectOnPlayChanged)

public:
    enum PlaybackState {
        Stopped = QMediaPlayer::StoppedState,
        Playing = QMediaPlayer::PlayingState,
        Paused = QMediaPlayer::PausedState
    };

    QnMjpegPlayer(QObject *parent = nullptr);
    ~QnMjpegPlayer();

    PlaybackState playbackState() const;

    QUrl source() const;

    QAbstractVideoSurface *videoSurface() const;

    int position() const;

    qint64 timestamp() const;

    qint64 finalTimestamp() const;
    void setFinalTimestamp(qint64 finalTimestamp);

    bool reconnectOnPlay() const;
    void setReconnectOnPlay(bool reconnectOnPlay);

public slots:
    void play();
    void pause();
    void stop();
    void setSource(const QUrl &source);
    void setVideoSurface(QAbstractVideoSurface *videoSurface);

signals:
    void playbackStateChanged();
    void sourceChanged();
    void videoSurfaceChanged();
    void positionChanged();
    void timestampChanged();
    void finalTimestampChanged();
    void reconnectOnPlayChanged();
    void playbackFinished();

private:
    QScopedPointer<QnMjpegPlayerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(QnMjpegPlayer)
};
