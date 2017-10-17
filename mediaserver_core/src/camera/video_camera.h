#ifndef __VIDEO_CAMERA_H__
#define __VIDEO_CAMERA_H__

#include <memory>

#include <QScopedPointer>

#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>

#include <api/helpers/thumbnail_request_data.h>
#include <core/resource/resource_consumer.h>
#include <core/dataprovider/live_stream_provider.h>
#include <server/server_globals.h>
#include <streaming/hls/hls_live_playlist_manager.h>

class QnVideoCameraGopKeeper;
class MediaStreamCache;
class MSSettings;

/**
 * TODO: #ak Have to introduce this class since QnAbstractVideoCamera 
 * is in common and thus cannot dependend on mediaserver's types.
 * TODO: #ak QnAbstractMediaServerVideoCamera is better be nx::vms::mediaserver::AbstractVideoCamera.
 */
class QnAbstractMediaServerVideoCamera:
    public QnAbstractVideoCamera
{
public:
    virtual ~QnAbstractMediaServerVideoCamera() override = default;

    virtual QnLiveStreamProviderPtr getLiveReader(
        QnServer::ChunksCatalog catalog,
        bool ensureInitialized = true) = 0;

    virtual int copyLastGop(
        bool primaryLiveStream,
        qint64 skipTime,
        QnDataPacketQueue& dstQueue,
        int cseq,
        bool iFramesOnly) = 0;

    virtual QnConstCompressedVideoDataPtr getLastVideoFrame(
        bool primaryLiveStream,
        int channel) const = 0;
    virtual QnConstCompressedAudioDataPtr getLastAudioFrame(bool primaryLiveStream) const = 0;

    virtual std::unique_ptr<QnConstDataPacketQueue> getFrameSequenceByTime(
        bool primaryLiveStream,
        qint64 time,
        int channel,
        QnThumbnailRequestData::RoundMethod roundMethod) const = 0;

    virtual void beforeStop() = 0;
    virtual bool isSomeActivity() const = 0;
    /**
     * Stop reading from camera if no active DataConsumers left.
     */
    virtual void stopIfNoActivity() = 0;
    virtual void updateActivity() = 0;

    /**
     * @return cache holding several last seconds of media stream. Can be null.
     */
    virtual const MediaStreamCache* liveCache(MediaQuality streamQuality) const = 0;
    virtual MediaStreamCache* liveCache(MediaQuality streamQuality) = 0;

    /**
     * TODO: #ak Should remove it from here.
     */
    virtual nx_hls::HLSLivePlaylistManagerPtr hlsLivePlaylistManager(
        MediaQuality streamQuality) const = 0;

    /**
     * Starts caching live stream, if not started.
     * @return true, if started, false if failed to start.
     */
    virtual bool ensureLiveCacheStarted(
        MediaQuality streamQuality,
        qint64 targetDurationUSec) = 0;
    virtual QnResourcePtr resource() const = 0;
};

typedef QnSharedResourcePointer<QnAbstractMediaServerVideoCamera> QnVideoCameraPtr;

class QnVideoCamera:
    public QObject,
    public QnAbstractMediaServerVideoCamera
{
    Q_OBJECT

public:
    QnVideoCamera(
        const MSSettings& settings,
        const QnResourcePtr& resource);
    virtual ~QnVideoCamera() override;

    virtual QnLiveStreamProviderPtr getLiveReader(
        QnServer::ChunksCatalog catalog,
        bool ensureInitialized = true) override;
    virtual QnLiveStreamProviderPtr getPrimaryReader() override;
    virtual QnLiveStreamProviderPtr getSecondaryReader() override;

    virtual int copyLastGop(
        bool primaryLiveStream,
        qint64 skipTime,
        QnDataPacketQueue& dstQueue,
        int cseq,
        bool iFramesOnly) override;

    virtual QnConstCompressedVideoDataPtr getLastVideoFrame(
        bool primaryLiveStream,
        int channel) const override;
    virtual QnConstCompressedAudioDataPtr getLastAudioFrame(bool primaryLiveStream) const override;

    virtual std::unique_ptr<QnConstDataPacketQueue> getFrameSequenceByTime(
        bool primaryLiveStream,
        qint64 time,
        int channel,
        QnThumbnailRequestData::RoundMethod roundMethod) const override;

    virtual void beforeStop() override;
    virtual bool isSomeActivity() const override;
    virtual void stopIfNoActivity() override;
    virtual void updateActivity() override;
    virtual void inUse(void* user) override;
    virtual void notInUse(void* user) override;

    virtual const MediaStreamCache* liveCache( MediaQuality streamQuality ) const override;
    virtual MediaStreamCache* liveCache( MediaQuality streamQuality ) override;
    virtual bool ensureLiveCacheStarted( MediaQuality streamQuality, qint64 targetDurationUSec ) override;
    virtual nx_hls::HLSLivePlaylistManagerPtr hlsLivePlaylistManager(MediaQuality streamQuality) const override;

    virtual QnResourcePtr resource() const override;

private:
    void createReader(QnServer::ChunksCatalog catalog);
    void stop();

private:
    QnMutex m_readersMutex;
    QnMutex m_getReaderMutex;
    const MSSettings& m_settings;
    QnResourcePtr m_resource;
    QnLiveStreamProviderPtr m_primaryReader;
    QnLiveStreamProviderPtr m_secondaryReader;

    QnVideoCameraGopKeeper* m_primaryGopKeeper;
    QnVideoCameraGopKeeper* m_secondaryGopKeeper;
    QSet<void*> m_cameraUsers;
    QnCompressedAudioDataPtr m_lastAudioFrame;
    //!index - is a \a MediaQuality element
    std::vector<std::unique_ptr<MediaStreamCache> > m_liveCache;
    //!index - is a \a MediaQuality element
    std::vector<nx_hls::HLSLivePlaylistManagerPtr> m_hlsLivePlaylistManager;

    const qint64 m_loStreamHlsInactivityPeriodMS;
    const qint64 m_hiStreamHlsInactivityPeriodMS;

    QnLiveStreamProviderPtr getLiveReaderNonSafe(QnServer::ChunksCatalog catalog, bool ensureInitialized);
    void startLiveCacheIfNeeded();
    bool ensureLiveCacheStarted(
        MediaQuality streamQuality,
        const QnLiveStreamProviderPtr& primaryReader,
        qint64 targetDurationUSec );
    QElapsedTimer m_lastActivityTimer;

private slots:
    void at_camera_resourceChanged();
};

#endif // __VIDEO_CAMERA_H__
