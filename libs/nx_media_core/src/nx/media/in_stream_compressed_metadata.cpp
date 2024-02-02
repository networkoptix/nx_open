// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "in_stream_compressed_metadata.h"

namespace nx::media {

InStreamCompressedMetadata::InStreamCompressedMetadata(QString codec, QByteArray metadata):
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

void InStreamCompressedMetadata::setExtraData(QByteArray extraData)
{
    m_extraData = std::move(extraData);
}

} // namespace nx::media
