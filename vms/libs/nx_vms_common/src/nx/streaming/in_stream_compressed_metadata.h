// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>

#include <nx/streaming/media_data_packet.h>

namespace nx::streaming {

class NX_VMS_COMMON_API InStreamCompressedMetadata: public QnCompressedMetadata
{
    using base_type = QnCompressedMetadata;
public:
    InStreamCompressedMetadata(QString codec, QByteArray metadata);

    virtual QString codec() const;

    virtual const char* extraData() const;

    virtual int extraDataSize() const;

    void setExtraData(QByteArray extraData);

private:
    QString m_codec;
    QByteArray m_extraData;
};

} // namespace nx::streaming
