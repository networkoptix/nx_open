#pragma once

#include <nx/streaming/video_data_packet.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>
#include <core/dataconsumer/abstract_data_receptor.h>

namespace nx {
namespace mediaserver {
namespace metadata {

class ResourceMetadataContext;
using namespace nx::sdk::metadata;

class VideoDataReceptor: public QnAbstractDataReceptor
{
public:
    using Callback = std::function<void(const QnCompressedVideoData*)>;

    VideoDataReceptor(Callback callback): m_callback(callback) {}
    virtual bool canAcceptData() const override { return true; }
    virtual void putData(const QnAbstractDataPacketPtr& data) override
    {
        if (auto video = dynamic_cast<const QnCompressedVideoData*>(data.get()))
            m_callback(video);
    }
private:
    Callback m_callback;
};
using VideoDataReceptorPtr = QSharedPointer<VideoDataReceptor>;

nxpt::ScopedRef<nx::sdk::metadata::CommonCompressedVideoPacket> toPluginVideoPacket(
    const QnCompressedVideoData* video,
    bool needDeepCopy);

} // namespace metadata
} // namespace mediaserver
} // namespace nx
