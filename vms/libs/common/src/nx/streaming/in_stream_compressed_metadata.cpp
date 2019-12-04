#include "in_stream_compressed_metadata.h"

namespace nx::streaming {

InStreamCompressedMetadata::InStreamCompressedMetadata(QString codec, nx::Buffer metadata):
    base_type(MetadataType::InStream, metadata.size()),
    m_codec(codec)
{
    setData(std::move(metadata));
}

QString InStreamCompressedMetadata::codec() const
{
    return m_codec;
}

const char* InStreamCompressedMetadata::extraData() const
{
    return m_extraData.constData();
}

int InStreamCompressedMetadata::extraDataSize() const
{
    return m_extraData.size();
}

void InStreamCompressedMetadata::setExtraData(nx::Buffer extraData)
{
    m_extraData = std::move(extraData);
}

} // namespace nx::streaming
