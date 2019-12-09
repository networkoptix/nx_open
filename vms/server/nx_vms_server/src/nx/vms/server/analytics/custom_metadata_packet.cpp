#include "custom_metadata_packet.h"

namespace nx::vms::server::analytics {

CustomMetadataPacket::CustomMetadataPacket(
    std::shared_ptr<nx::streaming::InStreamCompressedMetadata> compressedMetadata)
    :
    m_codec(compressedMetadata->codec().toStdString()),
    m_metadata(std::move(compressedMetadata))
{
}

const char* CustomMetadataPacket::codec() const
{
    return m_codec.c_str();
}

const char* CustomMetadataPacket::data() const
{
    return m_metadata->data();
}

int CustomMetadataPacket::dataSize() const
{
    return (int) m_metadata->dataSize();
}

const char* CustomMetadataPacket::contextData() const
{
    return m_metadata->extraData();
}

int CustomMetadataPacket::contextDataSize() const
{
    return m_metadata->extraDataSize();
}

int64_t CustomMetadataPacket::timestampUs() const
{
    return m_metadata->timestamp;
}

} // namespace nx::vms::server::analytics
