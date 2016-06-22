#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtMultimedia/QMediaPlayer>
#include <QtCore/QSize>

class QAbstractVideoSurface;

namespace nx {
namespace media {

class PlayerPrivate;

/**
 * Main facade class for the media player.
 */
class Player: public QObject
{
    Q_OBJECT

    Q_ENUMS(State)
    Q_ENUMS(MediaStatus)
    Q_ENUMS(VideoQuality)

    /**
     * Source url to open. In order to support multiserver archive, media player supports
     * non-standard URL scheme 'camera'. Example to open: "camera://media/<camera_id>".
     */
    Q_PROPERTY(QUrl source READ source WRITE setSource NOTIFY sourceChanged)

    /**
     * Video source to render decoded data
     */
    Q_PROPERTY(QAbstractVideoSurface* videoSurface READ videoSurface WRITE setVideoSurface NOTIFY videoSurfaceChanged)

    /**
     * Current playback UTC position at msec.
     */
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)

    /**
     * State defined by user action
     */
    Q_PROPERTY(State playbackState READ playbackState NOTIFY playbackStateChanged)

    /**
     * Current state defined by the internal implementation. For example, if the player has got the
     * 'play' command, 'mediaStatus' will be changed through 'buffering' to 'playing' state.
     */
    Q_PROPERTY(MediaStatus mediaStatus READ mediaStatus NOTIFY mediaStatusChanged)

    /**
     * The player is either on an archive or a live position.
     */
    Q_PROPERTY(bool liveMode READ liveMode NOTIFY liveModeChanged)

    Q_PROPERTY(int maxTextureSize READ maxTextureSize WRITE setMaxTextureSize)

    /**
     * Either one of enum VideoQuality values, or approximate vertical resolution.
     */
    Q_PROPERTY(int videoQuality READ videoQuality WRITE setVideoQuality)

    /**
     * Is (0, 0) if no video is playing or the resolution is not available.
     */
    Q_PROPERTY(QSize currentResolution READ currentResolution)

public:
    enum class State
    {
        Stopped,
        Playing,
        Paused,
    };

    enum class MediaStatus
    {
        Unknown,
        NoMedia,
        Loading,
        Loaded,

        Stalled,
        Buffering,
        Buffered,
        EndOfMedia,
        InvalidMedia,
    };

    enum VideoQuality
    {
        High = 0, //< Native stream, high quality.
        Low = 1, //< Native stream, low quality.
    };

public:
    Player(QObject *parent = nullptr);
    ~Player();

    State playbackState() const;

    MediaStatus mediaStatus() const;

    QUrl source() const;
    void setSource(const QUrl &source);

    QAbstractVideoSurface *videoSurface() const;
    void setVideoSurface(QAbstractVideoSurface *videoSurface);

    qint64 position() const;
    void setPosition(qint64 value);

    int maxTextureSize() const;
    void setMaxTextureSize(int value);

    bool reconnectOnPlay() const;
    void setReconnectOnPlay(bool reconnectOnPlay);

    bool liveMode() const;

    int videoQuality() const;
    void setVideoQuality(int videoQuality);

    QSize currentResolution() const;

public slots:
    void play();
    void pause();
    void stop();

signals:
    void playbackStateChanged();
    void sourceChanged();
    void videoSurfaceChanged();
    void positionChanged();
    void playbackFinished();
    void mediaStatusChanged();
    void reconnectOnPlayChanged();
    void liveModeChanged();

private:
    QScopedPointer<PlayerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(Player);
};

} // namespace media
} // namespace nx
