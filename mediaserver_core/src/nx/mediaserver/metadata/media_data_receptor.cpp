#include "media_data_receptor.h"

namespace nx {
namespace mediaserver {
namespace metadata {

nxpt::ScopedRef<nx::sdk::metadata::CommonCompressedVideoPacket> toPluginVideoPacket(
    const QnCompressedVideoData* video,
    bool needDeepCopy)
{
    nxpt::ScopedRef<CommonCompressedVideoPacket> packet(new CommonCompressedVideoPacket());

    packet->setTimestampUsec(video->timestamp);
    packet->setWidth(video->width);
    packet->setHeight(video->height);
    packet->setCodec(toString(video->compressionType).toStdString());
    if (needDeepCopy)
    {
        std::vector<char> buffer(video->dataSize());
        memcpy(&buffer[0], video->data(), video->dataSize());
        packet->setData(std::move(buffer));
    }
    else
    {
        packet->setData(video->data(), video->dataSize());
    }
    return packet;
}

} // namespace metadata
} // namespace mediaserver
} // namespace nx
