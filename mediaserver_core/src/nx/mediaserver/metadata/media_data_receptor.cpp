#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/common_compressed_video_packet.h>
#include <nx/sdk/metadata/abstract_consuming_metadata_manager.h>
#include <nx/streaming/video_data_packet.h>
#include "manager_pool.h"
#include "media_data_receptor.h"

namespace nx {
namespace mediaserver {
namespace metadata {

DataReceptor::DataReceptor(const ResourceMetadataContext* context):
    m_context(context)
{
}

void DataReceptor::putData(const QnAbstractDataPacketPtr& data)
{
    using namespace nx::sdk::metadata;
    nxpt::ScopedRef<AbstractConsumingMetadataManager> manager(
        (AbstractConsumingMetadataManager*)
        m_context->manager->queryInterface(IID_ConsumingMetadataManager), false);
    if (!manager)
        return;
    auto video = dynamic_cast<QnCompressedVideoData*> (data.get());
    if (video)
    {
        nxpt::ScopedRef<CommonCompressedVideoPacket> packet(new CommonCompressedVideoPacket());
        packet->setWidth(video->width);
        packet->setHeight(video->height);
        packet->setCodec(toString(video->compressionType).toStdString());
        std::vector<char> buffer(video->dataSize());
        memcpy(&buffer[0], video->data(), video->dataSize());
        packet->setData(std::move(buffer));

        manager->putData(packet.get());
    }
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
