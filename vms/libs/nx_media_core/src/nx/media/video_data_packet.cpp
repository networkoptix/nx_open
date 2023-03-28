// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "video_data_packet.h"

QnCompressedVideoData::QnCompressedVideoData( CodecParametersConstPtr ctx )
:
    QnAbstractMediaData( VIDEO ),
    width( -1 ),
    height( -1 ),
    pts( AV_NOPTS_VALUE )
{
    //useTwice = false;
    context = ctx;
    //ignore = false;
    flags = {};
}

void QnCompressedVideoData::assign(const QnCompressedVideoData* other)
{
    QnAbstractMediaData::assign(other);
    width = other->width;
    height = other->height;
    metadata = other->metadata;
    pts = other->pts;
}

QnWritableCompressedVideoData::QnWritableCompressedVideoData(
    size_t capacity,
    CodecParametersConstPtr ctx)
    :
    QnCompressedVideoData(ctx),
    m_data(CL_MEDIA_ALIGNMENT, capacity, AV_INPUT_BUFFER_PADDING_SIZE)
{
}

//!Implementation of QnAbstractMediaData::clone
QnWritableCompressedVideoData* QnWritableCompressedVideoData::clone() const
{
    auto rez = new QnWritableCompressedVideoData();
    rez->assign(this);
    return rez;
}

//!Implementation of QnAbstractMediaData::data
const char* QnWritableCompressedVideoData::data() const
{
    return m_data.data();
}

//!Implementation of QnAbstractMediaData::dataSize
size_t QnWritableCompressedVideoData::dataSize() const
{
    return m_data.size();
}

void QnWritableCompressedVideoData::setData(nx::utils::ByteArray&& buffer)
{
    m_data = std::move(buffer);
}

void QnWritableCompressedVideoData::assign(const QnWritableCompressedVideoData* other)
{
    QnCompressedVideoData::assign(other);
    m_data = other->m_data;
}
