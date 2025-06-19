// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>
#include <QtCore/QRect>
#include <QtCore/QSize>
#include <QtCore/QUrl>
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QVideoSink>

#include <core/resource/resource_fwd.h>
#include <nx/media/abstract_metadata_consumer.h>
#include <nx/media/media_fwd.h>
#include <nx/reflect/enum_instrument.h>
#include <nx/utils/impl_ptr.h>

class QnTimePeriodList;

// for tests
class QnArchiveStreamReader;

namespace nx::network::http { class Credentials; }
namespace nx::vms::common { class SystemContext; }

namespace nx::media {

struct NX_MEDIA_API PlayerStatistics
{
private:
    Q_GADGET
    Q_PROPERTY(qreal framerate MEMBER framerate CONSTANT)
    Q_PROPERTY(qreal bitrate MEMBER bitrate CONSTANT)
    Q_PROPERTY(QString codec MEMBER codec CONSTANT)
    Q_PROPERTY(bool isHwAccelerated MEMBER isHwAccelerated CONSTANT)
public:
    qreal framerate = 0.0;
    qreal bitrate = 0.0; //< Mbps.
    QString codec;
    bool isHwAccelerated = false;
};

class PlayerPrivate;

/**
 * Main facade class for the media player.
 */
class NX_MEDIA_API Player: public QObject
{
    Q_OBJECT

    /**
     * Video source to render decoded data
     */
    Q_PROPERTY(QVideoSink* videoSurface READ videoSurface WRITE setVideoSurface NOTIFY videoSurfaceChanged)

    /**
     * Current playback UTC position at msec.
     */
    Q_PROPERTY(qint64 position READ position WRITE setPosition NOTIFY positionChanged)

    /**
     * Playback speed factor. For example '2' - mean 2x play speed.
     */
    Q_PROPERTY(double speed READ speed WRITE setSpeed NOTIFY speedChanged)

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
     * Policy specifies when auto jump to the nearest following chunk is available.
     */
    Q_PROPERTY(AutoJumpPolicy autoJumpPolicy READ autoJumpPolicy WRITE setAutoJumpPolicy NOTIFY autoJumpPolicyChanged)

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

    Q_PROPERTY(bool allowHardwareAcceleration READ allowHardwareAcceleration WRITE setAllowHardwareAcceleration NOTIFY allowHardwareAccelerationChanged)

    /**
     * Is (0, 0) if no video is playing or the resolution is not available.
     */
    Q_PROPERTY(QSize currentResolution READ currentResolution NOTIFY currentResolutionChanged)

    Q_PROPERTY(QRect videoGeometry READ videoGeometry WRITE setVideoGeometry NOTIFY videoGeometryChanged)

    /**
     * Enable or disable audio playback. It has no effect if audio track is missing in source media.
     */
    Q_PROPERTY(bool audioEnabled READ isAudioEnabled WRITE setAudioEnabled NOTIFY audioEnabledChanged)

    Q_PROPERTY(bool audioOnlyMode READ isAudioOnlyMode NOTIFY audioOnlyModeChanged)

    Q_PROPERTY(Error error READ error NOTIFY errorChanged)

    /** Human-readable tag for logging and debugging purposes. */
    Q_PROPERTY(QString tag READ tag WRITE setTag NOTIFY tagChanged)

    Q_PROPERTY(bool allowSoftwareDecoderFallback
        READ allowSoftwareDecoderFallback
        WRITE setAllowSoftwareDecoderFallback
        NOTIFY allowSoftwareDecoderFallbackChanged)

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

    enum class AutoJumpPolicy
    {
        DefaultJumpPolicy, //< Tries to jump automatically to the nearest available chunk if state is not previewing.
        DisableJumpToNextChunk, //< Never tries to jump automatically to the nearest available chunk.
        DisableJumpToLive, //< Disable jump to LIVE in any mode.
        DisableJumpToLiveAndNextChunk, //< Disable jump to LIVE and to the nearest available chunk.
    };
    Q_ENUM(AutoJumpPolicy)

    NX_REFLECTION_ENUM_IN_CLASS(VideoQuality,
        UnknownVideoQuality = -1, //< There's no a quality which could be played by a player.
        HighVideoQuality = 0, //< Native stream, high quality.
        LowVideoQuality = 1, //< Native stream, low quality.
        LowIframesOnlyVideoQuality = 2, //< Native stream, low quality, I-frames only.
        CustomVideoQuality //< A number greater or equal to this is treated as a number of lines.
    )
    Q_ENUM(VideoQuality)

    enum TranscodingSupportStatus
    {
        TranscodingDisabled,
        TranscodingSupported,
        TranscodingNotSupported,
        TranscodingNotSupportedForServersOlder30,
    };
    Q_ENUM(TranscodingSupportStatus)

    enum class Error
    {
        NoError,
        TooManyConnections,
        CannotDecryptMedia,
    };
    Q_ENUM(Error)

public:
    Player(QObject *parent = nullptr);
    ~Player();

    QnResourcePtr resource() const;
    void setResource(const QnResourcePtr& value);

    State playbackState() const;

    MediaStatus mediaStatus() const;

    QVideoSink *videoSurface(int channel = 0) const;

    /**
     * Set video source to render specified video channel number
     */
    void setVideoSurface(QVideoSink *videoSink, int channel = 0);

    /**
     * Unset video source to render
     */
    void unsetVideoSurface(QVideoSink* videoSink, int channel = 0);

    qint64 position() const;
    /**
     * Position to play in UTC milliseconds
     */
    void setPosition(qint64 value);

    AutoJumpPolicy autoJumpPolicy() const;
    void setAutoJumpPolicy(AutoJumpPolicy policy);

    double speed() const;
    void setSpeed(double value);

    bool allowHardwareAcceleration() const;
    void setAllowHardwareAcceleration(bool value);

    int maxTextureSize() const;
    void setMaxTextureSize(int value);

    bool reconnectOnPlay() const;
    void setReconnectOnPlay(bool reconnectOnPlay);

    void setPlaybackMask(const QnTimePeriodList& periods);
    Q_INVOKABLE void setPlaybackMask(qint64 startTimeMs, qint64 durationMs);

    /**
     * Set playback mask with list of @param periods. Each period in the list represents
     * QJsonObject with fields 'startTimeMs' and 'durationMs'.
     */
    Q_INVOKABLE void setPlaybackMask(const QVariantList& periods);

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
    bool isAudioOnlyMode() const;

    RenderContextSynchronizerPtr renderContextSynchronizer() const;
    void setRenderContextSynchronizer(RenderContextSynchronizerPtr value);

    /**
     * Add new metadata consumer.
     * @return True if success, false if specified consumer already exists.
     */
    bool addMetadataConsumer(const AbstractMetadataConsumerPtr& metadataConsumer);

    /**
     * Remove metadata consumer.
     * @return True if success, false if specified consumer is not found.
     */
    bool removeMetadataConsumer(const AbstractMetadataConsumerPtr& metadataConsumer);

    Error error() const;

    QString tag() const;
    void setTag(const QString& value);

    Q_INVOKABLE void invalidateServer();

    void setAllowSoftwareDecoderFallback(bool value);
    bool allowSoftwareDecoderFallback() const;

public slots:
    void play();
    void pause();

    /**
     * Go to a next video frame. Method accessible in pause mode only.
     * Otherwise it does nothing.
     */
    void nextFrame();

    /**
     * Go to a previous video frame. Method accessible in pause mode only.
     * Otherwise it does nothing.
     */
    void previousFrame();

    /**
     * Preview mode similar to pause mode but optimized for fast seek.
     */
    void preview();
    void stop();

signals:
    void playbackStateChanged();
    void resourceChanged();
    void videoSurfaceChanged();
    void positionChanged();
    void playbackFinished();
    void mediaStatusChanged();
    void reconnectOnPlayChanged();
    void liveModeChanged();
    void aspectRatioChanged();
    void videoQualityChanged();
    void allowOverlayChanged();
    void allowHardwareAccelerationChanged();
    void videoGeometryChanged();
    void currentResolutionChanged();
    void audioEnabledChanged();
    void tagChanged();
    void speedChanged();
    void autoJumpPolicyChanged();
    void audioOnlyModeChanged();
    void allowSoftwareDecoderFallbackChanged();
    void errorChanged();

protected:
    virtual void setResourceInternal(const QnResourcePtr& resource);

    //Function for tests.
    void testSetOwnedArchiveReader(QnArchiveStreamReader* archiveReader);

private:
    bool checkReadyToPlay();

private:
    class Private;
    nx::utils::ImplPtr<Private> d;
};

/**
 * @param videoQuality Either one of the enum Player::VideoQuality values, or the video frame
 *     height in pixels, which denotes a custom quality. Note that to denote the frame height, the
 *     value must be greater or equal to Player::VideoQuality::CustomVideoQuality.
 */
NX_MEDIA_API QString videoQualityToString(int videoQuality);

} // namespace nx::media
