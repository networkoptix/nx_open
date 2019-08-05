#pragma once

#include <memory>
#include <utility>
#include <QScopedPointer>

#include <camera/abstract_video_camera.h>
#include <nx/utils/elapsed_timer.h>

class QnDataProviderFactory;
class MediaStreamCache;

namespace nx::vms::server {

class Settings;
class GopKeeper;

class VideoCamera: public QObject, public nx::vms::server::AbstractVideoCamera
{
public:
    VideoCamera(
        const nx::vms::server::Settings& settings,
        QnDataProviderFactory* dataProviderFactory,
        const nx::vms::server::resource::CameraPtr& camera);
    virtual ~VideoCamera() override;

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
    nx::vms::server::resource::CameraPtr m_camera;
    QnLiveStreamProviderPtr m_primaryReader;
    QnLiveStreamProviderPtr m_secondaryReader;

    nx::vms::server::GopKeeper* m_primaryGopKeeper;
    nx::vms::server::GopKeeper* m_secondaryGopKeeper;
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

    ForceLiveCacheForPrimaryStream getSettingForceLiveCacheForPrimaryStream() const;

    bool isLiveCacheForcingUseful(QString* outReasonForLog) const;
    bool needToForceLiveCacheForPrimaryStream(QString* outReasonForLog) const;
    bool isLiveCacheNeededForPrimaryStream(QString* outReasonForLog) const;

    nx::vms::server::GopKeeper* getGopKeeper(StreamIndex streamIndex) const;

    QnLiveStreamProviderPtr getLiveReaderNonSafe(
        QnServer::ChunksCatalog catalog, bool ensureInitialized);

    std::pair<int, int> getMinMaxLiveCacheSizeMs(MediaQuality streamQuality) const;

    void startLiveCacheIfNeeded();

    bool ensureLiveCacheStarted(
        MediaQuality streamQuality,
        const QnLiveStreamProviderPtr& reader,
        qint64 targetDurationUSec,
        const QString& reasonForLog);

    void at_camera_resourceChanged();
};

} // namespace nx::vms::server
