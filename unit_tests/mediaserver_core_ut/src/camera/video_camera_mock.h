#pragma once

#include <atomic>

#include <camera/video_camera.h>

/**
 * Validates QnAbstractMediaServerVideoCamera interface is used in a proper way.
 * Almost every method has an empty implementation.
 */
class MediaServerVideoCameraMock:
    public QnAbstractMediaServerVideoCamera
{
public:
    MediaServerVideoCameraMock();

    virtual QSharedPointer<QnLiveStreamProvider> getPrimaryReader() override;
    virtual QSharedPointer<QnLiveStreamProvider> getSecondaryReader() override;
    virtual QnLiveStreamProviderPtr getLiveReader(
        QnServer::ChunksCatalog catalog,
        bool ensureInitialized = true) override;

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
        nx::api::ImageRequest::RoundMethod roundMethod) const override;

    virtual void beforeStop() override;
    virtual bool isSomeActivity() const override;
    virtual void stopIfNoActivity() override;
    virtual void updateActivity() override;
    virtual void inUse(void* user) override;
    virtual void notInUse(void* user) override;

    virtual const MediaStreamCache* liveCache(MediaQuality streamQuality) const override;
    virtual MediaStreamCache* liveCache(MediaQuality streamQuality) override;

    virtual nx::mediaserver::hls::LivePlaylistManagerPtr hlsLivePlaylistManager(
        MediaQuality streamQuality) const override;

    virtual bool ensureLiveCacheStarted(
        MediaQuality streamQuality,
        qint64 targetDurationUSec) override;
    virtual QnResourcePtr resource() const override;

private:
    std::atomic<int> m_usageCounter;
};
