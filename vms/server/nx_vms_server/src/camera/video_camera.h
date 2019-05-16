#pragma once

#include <memory>
#include <utility>

#include <QScopedPointer>

#include <nx/streaming/abstract_data_consumer.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/abstract_media_stream_data_provider.h>


#include <nx/api/mediaserver/image_request.h>

#include "camera_fwd.h"
#include <core/resource/resource_consumer.h>
#include <providers/live_stream_provider.h>
#include <server/server_globals.h>
#include <nx/vms/server/settings.h>
#include <nx/vms/server/hls/hls_live_playlist_manager.h>
#include <api/helpers/thumbnail_request_data.h>

#include <nx/utils/elapsed_timer.h>

class QnVideoCameraGopKeeper;
class QnDataProviderFactory;
class MediaStreamCache;

namespace nx { namespace vms::server { class Settings; } }

/**
 * TODO: #ak Have to introduce this class since QnAbstractVideoCamera
 * is in common and thus cannot dependend on mediaserver's types.
 * TODO: #ak QnAbstractMediaServerVideoCamera is better be nx::vms::vms::server::AbstractVideoCamera.
 */
class QnAbstractMediaServerVideoCamera: public QnAbstractVideoCamera
{
public:
    using StreamIndex = nx::vms::api::StreamIndex;

    virtual ~QnAbstractMediaServerVideoCamera() override = default;

    virtual QnLiveStreamProviderPtr getLiveReader(
        QnServer::ChunksCatalog catalog,
        bool ensureInitialized = true, bool createIfNotExist = true) = 0;

    QnLiveStreamProviderPtr getPrimaryReader() { return getLiveReader(QnServer::HiQualityCatalog); }
    QnLiveStreamProviderPtr getSecondaryReader() { return getLiveReader(QnServer::LowQualityCatalog); }

    virtual int copyLastGop(
        StreamIndex streamIndex,
        qint64 skipTime,
        QnDataPacketQueue& dstQueue,
        bool iFramesOnly) = 0;

    virtual QnConstCompressedVideoDataPtr getLastVideoFrame(
        StreamIndex streamIndex,
        int channel) const = 0;
    virtual QnConstCompressedAudioDataPtr getLastAudioFrame(StreamIndex streamIndex) const = 0;

    /**
     * @return I-frame and the following P-frames up to the desired frame. Can be null but not
     * empty.
     */
    virtual std::unique_ptr<QnConstDataPacketQueue> getFrameSequenceByTime(
        StreamIndex streamIndex,
        qint64 time,
        int channel,
        nx::api::ImageRequest::RoundMethod roundMethod) const = 0;

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
    virtual nx::vms::server::hls::LivePlaylistManagerPtr hlsLivePlaylistManager(
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

class QnVideoCamera: public QObject, public QnAbstractMediaServerVideoCamera
{
    Q_OBJECT

public:
    QnVideoCamera(
        const nx::vms::server::Settings& settings,
        QnDataProviderFactory* dataProviderFactory,
        const QnResourcePtr& resource);
    virtual ~QnVideoCamera() override;

    virtual QnLiveStreamProviderPtr getLiveReader(
        QnServer::ChunksCatalog catalog,
        bool ensureInitialized = true, bool createIfNotExist = true) override;

    virtual int copyLastGop(
        StreamIndex streamIndex,
        qint64 skipTime,
        QnDataPacketQueue& dstQueue,
        bool iFramesOnly) override;

    virtual QnConstCompressedVideoDataPtr getLastVideoFrame(
        StreamIndex streamIndex,
        int channel) const override;

    virtual QnConstCompressedAudioDataPtr getLastAudioFrame(
        StreamIndex streamIndex) const override;

    virtual std::unique_ptr<QnConstDataPacketQueue> getFrameSequenceByTime(
        StreamIndex streamIndex,
        qint64 time,
        int channel,
        nx::api::ImageRequest::RoundMethod roundMethod) const;

    virtual void beforeStop() override;
    virtual bool isSomeActivity() const override;
    virtual void stopIfNoActivity() override;
    virtual void updateActivity() override;
    virtual void inUse(void* user) override;
    virtual void notInUse(void* user) override;

    virtual const MediaStreamCache* liveCache( MediaQuality streamQuality ) const override;
    virtual MediaStreamCache* liveCache( MediaQuality streamQuality ) override;
    virtual bool ensureLiveCacheStarted( MediaQuality streamQuality, qint64 targetDurationUSec ) override;
    virtual nx::vms::server::hls::LivePlaylistManagerPtr hlsLivePlaylistManager(MediaQuality streamQuality) const override;

    virtual QnResourcePtr resource() const override;

private:
    void createReader(QnServer::ChunksCatalog catalog);
    QnLiveStreamProviderPtr readerByQuality(MediaQuality streamQuality) const;
    void resetCachesIfNeeded(MediaQuality streamQuality);
    void stop();

private:
    QnMutex m_readersMutex;
    QnMutex m_getReaderMutex;
    const nx::vms::server::Settings& m_settings;
    QnDataProviderFactory* m_dataProviderFactory = nullptr;
    QnResourcePtr m_resource;
    QnLiveStreamProviderPtr m_primaryReader;
    QnLiveStreamProviderPtr m_secondaryReader;

    QnVideoCameraGopKeeper* m_primaryGopKeeper;
    QnVideoCameraGopKeeper* m_secondaryGopKeeper;
    QSet<void*> m_cameraUsers;
    QnCompressedAudioDataPtr m_lastAudioFrame;
    //!index - is a \a MediaQuality element
    std::vector<std::unique_ptr<MediaStreamCache>> m_liveCache;
    std::map<MediaQuality, nx::utils::ElapsedTimer> m_liveCacheValidityTimers;
    //!index - is a \a MediaQuality element
    std::vector<nx::vms::server::hls::LivePlaylistManagerPtr> m_hlsLivePlaylistManager;

    const qint64 m_loStreamHlsInactivityPeriodMS;
    const qint64 m_hiStreamHlsInactivityPeriodMS;

    QElapsedTimer m_lastActivityTimer;

private:
    enum class ForceLiveCacheForPrimaryStream { no, yes, auto_ };

    ForceLiveCacheForPrimaryStream getSettingForceLiveCacheForPrimaryStream(
        const QnSecurityCamResource* cameraResource) const;

    bool isLiveCacheForcingUseful(QString* outReasonForLog) const;

    bool needToForceLiveCacheForPrimaryStream(
        const QnSecurityCamResource* cameraResource, QString* outReasonForLog) const;

    bool isLiveCacheNeededForPrimaryStream(
        const QnSecurityCamResource* cameraResource, QString* outReasonForLog) const;

    QnVideoCameraGopKeeper* getGopKeeper(StreamIndex streamIndex) const;

    QnLiveStreamProviderPtr getLiveReaderNonSafe(
        QnServer::ChunksCatalog catalog, bool ensureInitialized);

    std::pair<int, int> getMinMaxLiveCacheSizeMs(MediaQuality streamQuality) const;

    void startLiveCacheIfNeeded();

    bool ensureLiveCacheStarted(
        MediaQuality streamQuality,
        const QnLiveStreamProviderPtr& reader,
        qint64 targetDurationUSec,
        const QString& reasonForLog);

private slots:
    void at_camera_resourceChanged();
};
