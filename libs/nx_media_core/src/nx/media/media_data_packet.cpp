// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_data_packet.h"

#include <nx/media/config.h>
#include <nx/media/motion_detection.h>
#include <nx/utils/bit_stream.h>

#if defined(Q_OS_MACOS) && defined(__amd64)
#include <smmintrin.h>
#endif

//----------------------------------- QnAbstractMediaData ----------------------------------------

bool isLowMediaQuality(MediaQuality q)
{
    return q == MEDIA_Quality_Low || q == MEDIA_Quality_LowIframesOnly || q == MEDIA_Quality_ForceLow;
}

QnAbstractMediaData::QnAbstractMediaData( DataType _dataType ):
    dataProvider(nullptr),
    dataType(_dataType),
    compressionType(AV_CODEC_ID_NONE),
    flags(MediaFlags_None),
    channelNumber(0),
    context(0),
    opaque(0)
{
}

QnAbstractMediaData::~QnAbstractMediaData()
{
}

void QnAbstractMediaData::assign(const QnAbstractMediaData* other)
{
    dataProvider = other->dataProvider;
    timestamp = other->timestamp;
    dataType = other->dataType;
    compressionType = other->compressionType;
    flags = other->flags;
    channelNumber = other->channelNumber;
    context = other->context;
    opaque = other->opaque;
    encryptionData = other->encryptionData;
}

bool QnAbstractMediaData::isLQ() const
{
    return flags & MediaFlags_LowQuality;
}

bool QnAbstractMediaData::isLive() const
{
    return flags & MediaFlags_LIVE;
}

AVMediaType toAvMediaType(QnAbstractMediaData::DataType dataType)
{
    switch (dataType)
    {
        case QnAbstractMediaData::DataType::AUDIO:
            return AVMEDIA_TYPE_AUDIO;
        case QnAbstractMediaData::DataType::CONTAINER:
        case QnAbstractMediaData::DataType::EMPTY_DATA:
        case QnAbstractMediaData::DataType::META_V1: //< Because it's deprecated.
            return AVMEDIA_TYPE_UNKNOWN;
        case QnAbstractMediaData::GENERIC_METADATA:
            return AVMEDIA_TYPE_SUBTITLE;
        case QnAbstractMediaData::DataType::VIDEO:
            return AVMEDIA_TYPE_VIDEO;
        default:
            return AVMEDIA_TYPE_UNKNOWN;
    }

    return AVMEDIA_TYPE_UNKNOWN;
}

//------------------------------------- QnEmptyMediaData -----------------------------------------

QnEmptyMediaData::QnEmptyMediaData():
    QnAbstractMediaData(EMPTY_DATA),
    m_data(CL_MEDIA_ALIGNMENT, 0, AV_INPUT_BUFFER_PADDING_SIZE)
{
}

QnEmptyMediaData* QnEmptyMediaData::clone() const
{
    auto rez = new QnEmptyMediaData();
    rez->assign(this);
    rez->m_data = m_data;
    return rez;
}

const char* QnEmptyMediaData::data() const
{
    return m_data.data();
}

size_t QnEmptyMediaData::dataSize() const
{
    return m_data.size();
}

void QnEmptyMediaData::setData(nx::utils::ByteArray&& buffer)
{
    m_data = std::move(buffer);
}
