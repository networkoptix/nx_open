#pragma once

#include <server/server_globals.h>

#include <nx/api/mediaserver/image_request.h>
#include <nx/vms/server/hls/hls_live_playlist_manager.h>
#include <providers/live_stream_provider.h>
#include <nx/streaming/data_packet_queue.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/audio_data_packet.h>

#include <core/dataprovider/abstract_video_camera.h>
#include <camera/camera_fwd.h>

namespace nx::vms::server {

// TODO: #dliman should be merged with QnAbstractVideoCamera from common.
class AbstractVideoCamera: public QnAbstractVideoCamera
{
public:
    using StreamIndex = nx::vms::api::StreamIndex;

    virtual ~AbstractVideoCamera() override = default;

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

} // namespace nx::vms::server
