#pragma once

#include <nx/utils/buffer.h>

#include <nx/streaming/media_data_packet.h>

namespace nx::streaming {

class InStreamCompressedMetadata: public QnCompressedMetadata
{
    using base_type = QnCompressedMetadata;
public:
    InStreamCompressedMetadata(QString codec, nx::Buffer metadata);

    virtual QString codec() const;

    virtual const char* extraData() const;

    virtual int extraDataSize() const;

    void setExtraData(nx::Buffer extraData);

private:
    QString m_codec;
    nx::Buffer m_extraData;
};

} // namespace nx::streaming
