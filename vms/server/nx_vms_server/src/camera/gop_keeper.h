#pragma once

#include <deque>

#include <nx/streaming/audio_data_packet.h>
#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/abstract_data_consumer.h>

#include <core/resource/resource_consumer.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/server/resource/resource_fwd.h>

#include <nx/api/mediaserver/image_request.h>
#include <server/server_globals.h>

namespace nx {
namespace vms {
namespace server {

class VideoCamera;

class GopKeeper: public QnResourceConsumer, public QnAbstractDataConsumer
{
public:
    GopKeeper(VideoCamera* camera, const QnResourcePtr &resource, QnServer::ChunksCatalog catalog);

    virtual ~GopKeeper();

    virtual void beforeDisconnectFromResource();

    int copyLastGop(qint64 skipTime, QnDataPacketQueue& dstQueue, bool iFramesOnly);

    virtual bool canAcceptData() const;
    virtual void putData(const QnAbstractDataPacketPtr& data);
    virtual bool processData(const QnAbstractDataPacketPtr& data);

    QnConstCompressedVideoDataPtr getLastVideoFrame(int channel) const;
    QnConstCompressedAudioDataPtr getLastAudioFrame() const;

    void updateCameraActivity();
    virtual bool needConfigureProvider() const override { return false; }
    void clearVideoData();

    std::unique_ptr<QnConstDataPacketQueue> getFrameSequenceByTime(
        qint64 timeUs, int channel, nx::api::ImageRequest::RoundMethod roundMethod) const;

private:
    QnConstCompressedVideoDataPtr getIframeByTimeUnsafe(
        qint64 time, int channel, nx::api::ImageRequest::RoundMethod roundMethod) const;

    QnConstCompressedVideoDataPtr getIframeByTime(
        qint64 time, int channel, nx::api::ImageRequest::RoundMethod roundMethod) const;

    std::unique_ptr<QnConstDataPacketQueue> getGopTillTime(qint64 time, int channel) const;

private:
    mutable QnMutex m_queueMtx;
    int m_lastKeyFrameChannel;
    QnConstCompressedAudioDataPtr m_lastAudioData;
    QnConstCompressedVideoDataPtr m_lastKeyFrame[CL_MAX_CHANNELS];
    std::deque<QnConstCompressedVideoDataPtr> m_lastKeyFrames[CL_MAX_CHANNELS];
    int m_gotIFramesMask;
    int m_allChannelsMask;
    bool m_isSecondaryStream;
    VideoCamera* m_camera;
    bool m_activityStarted;
    qint64 m_activityStartTime;
    QnServer::ChunksCatalog m_catalog;
    qint64 m_nextMinTryTime;
};

} // namespace server
} // namespace vms
} // namespace nx
