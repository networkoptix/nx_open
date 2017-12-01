#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtMultimedia/QMediaPlayer>
#include <QtCore/QSize>
#include <QtCore/QRect>

#include <nx/fusion/model_functions_fwd.h>

class QAbstractVideoSurface;

// for tests
class QnArchiveStreamReader;
class QnResource;
template<class Resource> class QnSharedResourcePointer;
typedef QnSharedResourcePointer<QnResource> QnResourcePtr;
class QnCommonModule;

namespace nx {
namespace media {

struct PlayerStatistics
{
private:
    Q_GADGET
    Q_PROPERTY(qreal framerate MEMBER framerate CONSTANT)
    Q_PROPERTY(qreal bitrate MEMBER bitrate CONSTANT)
    Q_PROPERTY(QString codec MEMBER codec CONSTANT)
    Q_PROPERTY(bool isHwAccelerated MEMBER isHwAccelerated CONSTANT)
public:
    qreal framerate = 0.0;
    qreal bitrate = 0.0;
    QString codec;
    bool isHwAccelerated = false;
};

class PlayerPrivate;

/**
 * Main facade class for the media player.
 */
class Player: public QObject
{
    Q_OBJECT

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

    /**
     * Video aspect ratio. In some cases it may differs from frame width / height
     */
    Q_PROPERTY(double aspectRatio READ aspectRatio NOTIFY aspectRatioChanged)

    Q_PROPERTY(int maxTextureSize READ maxTextureSize WRITE setMaxTextureSize)

    /**
     * Either one of enum VideoQuality values, or approximate vertical resolution.
     */
    Q_PROPERTY(int videoQuality READ videoQuality WRITE setVideoQuality NOTIFY videoQualityChanged)

    Q_PROPERTY(bool allowOverlay READ allowOverlay WRITE setAllowOverlay NOTIFY allowOverlayChanged)

    /**
     * Is (0, 0) if no video is playing or the resolution is not available.
     */
    Q_PROPERTY(QSize currentResolution READ currentResolution NOTIFY currentResolutionChanged)

    Q_PROPERTY(QRect videoGeometry READ videoGeometry WRITE setVideoGeometry NOTIFY videoGeometryChanged)

    /**
     * Enable or disable audio playback. It has no effect if audio track is missing in source media.
     */
    Q_PROPERTY(bool audioEnabled READ isAudioEnabled WRITE setAudioEnabled NOTIFY audioEnabledChanged)

public:
    enum class State
    {
        Stopped,
        Playing,
        Paused,
        Previewing,
    };
    Q_ENUM(State)

    enum class MediaStatus
    {
        Unknown,
        NoVideoStreams,
        NoMedia,
        Loading,
        Loaded,

        Stalled,
        Buffering,
        Buffered,
        EndOfMedia,
        InvalidMedia,
    };
    Q_ENUM(MediaStatus)

    enum VideoQuality
    {
        UnknownVideoQuality = -1, //< There's no a quality which could be played by a player.
        HighVideoQuality = 0, //< Native stream, high quality.
        LowVideoQuality = 1, //< Native stream, low quality.
        LowIframesOnlyVideoQuality = 2, //< Native stream, low quality, I-frames only.
        CustomVideoQuality //< A number greater or equal to this is treated as a number of lines.
    };
    Q_ENUM(VideoQuality)

    enum TranscodingSupportStatus
    {
        TranscodingDisabled,
        TranscodingSupported,
        TranscodingNotSupported,
        TranscodingNotSupportedForServersOlder30,
        TranscodingNotSupportedForArmServers,
    };
    Q_ENUM(TranscodingSupportStatus)

public:
    Player(QObject *parent = nullptr);
    ~Player();

    State playbackState() const;

    MediaStatus mediaStatus() const;

    QUrl source() const;
    /**
     * Set media source to play.
     * Supported types of media sources (url scheme part):
     * 'file' - local file
     * any other scheme interpreted as a link to a resource in resourcePool. Url path is resource Id.
     */
    void setSource(const QUrl &source);

    QAbstractVideoSurface *videoSurface(int channel = 0) const;

    /**
     * Set video source to render specified video channel number
     */
    void setVideoSurface(QAbstractVideoSurface *videoSurface, int channel = 0);

    qint64 position() const;
    /**
     * Position to play in UTC milliseconds
     */
    void setPosition(qint64 value);

    int maxTextureSize() const;
    void setMaxTextureSize(int value);

    bool reconnectOnPlay() const;
    void setReconnectOnPlay(bool reconnectOnPlay);

    /**
     * @return True if current playback position is 'now' time (live video from a camera).
     * For non-camera sources like local files it is already false.
     */
    bool liveMode() const;
    double aspectRatio() const;

    int videoQuality() const;
    void setVideoQuality(int videoQuality);

    /**
     * @return Video quality of the video stream being played back. May differ from videoQuality().
     */
    Q_INVOKABLE VideoQuality actualVideoQuality() const;

    /**
     * @return Filtered list of available video qualities.
     * Video quality is treated as available if its vertical resolution does not exceed
     * vertical resolution of high quality stream (or low quality stream if HQ stream is
     * unavailable).
     */
    Q_INVOKABLE QList<int> availableVideoQualities(const QList<int>& videoQualities) const;

    bool allowOverlay() const;
    void setAllowOverlay(bool allowOverlay);

    QSize currentResolution() const;

    Q_INVOKABLE TranscodingSupportStatus transcodingStatus() const;

    QRect videoGeometry() const;
    void setVideoGeometry(const QRect& rect);

    // Namespace specification is required for QML.
    Q_INVOKABLE nx::media::PlayerStatistics currentStatistics() const;

    bool isAudioEnabled() const;
    void setAudioEnabled(bool value);

public slots:
    void play();
    void pause();

    /**
     * Preview mode similar to pause mode but optimized for fast seek.
     */
    void preview();
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
    void aspectRatioChanged();
    void videoQualityChanged();
    void allowOverlayChanged();
    void videoGeometryChanged();
    void currentResolutionChanged();
    void audioEnabledChanged();

protected: //< for tests
    void testSetOwnedArchiveReader(QnArchiveStreamReader* archiveReader);
    void testSetCamera(const QnResourcePtr& camera);

    virtual QnCommonModule* commonModule() const = 0;

private:
    bool checkReadyToPlay();

private:
    QScopedPointer<PlayerPrivate> d_ptr;
    Q_DECLARE_PRIVATE(Player)
};

QN_FUSION_DECLARE_FUNCTIONS(Player::VideoQuality, (lexical))

} // namespace media
} // namespace nx

Q_DECLARE_METATYPE(nx::media::PlayerStatistics)
