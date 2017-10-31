#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>
#include <nx/sdk/metadata/abstract_consuming_metadata_manager.h>
#include <nx/streaming/video_data_packet.h>
#include "manager_pool.h"
#include "media_data_receptor.h"

namespace nx {
namespace mediaserver {
namespace metadata {

VideoDataReceptor::VideoDataReceptor(const ResourceMetadataContext* context):
    m_context(context)
{
}

void VideoDataReceptor::detachFromContext()
{
    QnMutexLocker lock(&m_mutex);
    m_context = nullptr;
}

void VideoDataReceptor::putData(const QnAbstractDataPacketPtr& data)
{
    QnMutexLocker lock(&m_mutex);
    if (!m_context)
        return;
    using namespace nx::sdk::metadata;
    nxpt::ScopedRef<AbstractConsumingMetadataManager> manager(
        (AbstractConsumingMetadataManager*)
        m_context->manager()->queryInterface(IID_ConsumingMetadataManager), false);
    if (!manager)
        return;
    auto video = dynamic_cast<QnCompressedVideoData*> (data.get());
    if (video)
    {
        nxpt::ScopedRef<CommonCompressedVideoPacket> packet(new CommonCompressedVideoPacket());
        packet->setTimestampUsec(data->timestamp);
        packet->setWidth(video->width);
        packet->setHeight(video->height);
        packet->setCodec(toString(video->compressionType).toStdString());
        if (m_context->pluginManifest().capabilities.testFlag(
            nx::api::AnalyticsDriverManifestBase::needDeepCopyForMediaFrame))
        {
            std::vector<char> buffer(video->dataSize());
            memcpy(&buffer[0], video->data(), video->dataSize());
            packet->setData(std::move(buffer));
        }
        else
        {
            packet->setData(video->data(), video->dataSize());
        }

        manager->putData(packet.get());
    }
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
