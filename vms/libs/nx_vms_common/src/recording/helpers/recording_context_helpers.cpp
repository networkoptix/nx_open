// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "recording_context_helpers.h"

#include <QtCore/QDataStream>
#include <QtCore/QtEndian>

#include <nx/codec/hevc/hevc_common.h>
#include <nx/codec/nal_units.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/media/h264_utils.h>
#include <nx/media/utils.h>
#include <nx/utils/log/log.h>

extern "C" {
#include <libavformat/avformat.h>
} // extern "C"

namespace nx::recording::helpers {

bool addStream(const CodecParametersConstPtr& codecParameters, AVFormatContext* formatContext)
{
    if (!codecParameters || !formatContext)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to add media stream, invalid input params!");
        return false;
    }

    auto avCodecParams = codecParameters->getAvCodecParameters();

    AVStream* stream = avformat_new_stream(formatContext, nullptr);
    if (stream == 0)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to allocate AVStream: out of memory");
        return false;
    }
    stream->id = formatContext->nb_streams - 1;

    if (avCodecParams->codec_type == AVMEDIA_TYPE_VIDEO)
    {
        AVRational defaultFrameRate{1, 60};
        stream->time_base = defaultFrameRate;
    }

    if (avcodec_parameters_copy(stream->codecpar, avCodecParams) < 0)
    {
        NX_ERROR(NX_SCOPE_TAG, "Failed to copy AVCodecParameters, out of memory");
        return false;
    }

    // FFmpeg workarounds. TODO: Check if it is still needed.
    if (stream->codecpar->codec_id == AV_CODEC_ID_MP3)
    {
        // avoid FFMPEG bug for MP3 mono. block_align hard-coded inside ffmpeg for stereo channels and it is cause problem
        if (stream->codecpar->ch_layout.nb_channels == 1)
            stream->codecpar->block_align = 0;
        // Fill frame_size for MP3 (it is a constant). AVI container works wrong without it.
        if (stream->codecpar->frame_size == 0)
            stream->codecpar->frame_size = QnFfmpegHelper::getDefaultFrameSize(stream->codecpar);
    }

    // TODO: #lbusygin Add frame_size to serialization/deserialization functions for CodecParameters.
    if (stream->codecpar->codec_id == AV_CODEC_ID_AAC)
    {
        if (stream->codecpar->frame_size == 0)
            stream->codecpar->frame_size = QnFfmpegHelper::getDefaultFrameSize(stream->codecpar);
    }

    stream->codecpar->codec_tag = 0;
    // Force video tag due to ffmpeg missing it out for h265 in AVI.
    if (stream->codecpar->codec_id == AV_CODEC_ID_H265 && strcmp(formatContext->oformat->name, "avi") == 0)
        stream->codecpar->codec_tag = MKTAG('H', 'E', 'V', 'C');

    return true;
}

QByteArray serializeMetadataPacket(const QnConstAbstractCompressedMetadataPtr& data)
{
    QByteArray result;
    if (auto motionPacket = std::dynamic_pointer_cast<const QnMetaDataV1>(data))
    {
        QDataStream stream(&result, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);
        stream << data->metadataType;
        stream << motionPacket->serialize();
        return result;
    }

    static const int kHeaderSize = 8;
    result.resize(data->dataSize() + kHeaderSize);
    char* buffer = result.data();
    uint32_t* metadataTypePtr = (uint32_t*) buffer;
    *metadataTypePtr = qToLittleEndian((uint32_t) data->metadataType);
    metadataTypePtr[1] = qToLittleEndian(data->dataSize());
    memcpy(buffer + kHeaderSize, data->data(), data->dataSize());
    return result;
}

QnAbstractCompressedMetadataPtr deserializeMetaDataPacket(const QByteArray& data)
{
    QDataStream stream(data);
    stream.setByteOrder(QDataStream::LittleEndian);
    MetadataType type;
    QByteArray payload;
    stream >> type;
    stream >> payload;

    switch (type)
    {
        case MetadataType::Motion:
        {
            QnMetaDataV1Light motionData;
            memcpy(&motionData, payload.constData(), sizeof(motionData));
            motionData.doMarshalling();
            return QnMetaDataV1::fromLightData(motionData);
        }
        break;
        case MetadataType::ObjectDetection:
        {
            auto result = std::make_shared<QnCompressedMetadata>(
                MetadataType::ObjectDetection, payload.size());

            result->m_data.write(payload);
            return result;
        }
        break;
        default:
            NX_ASSERT(false, "Unexpected metadata type");
    }

    return QnAbstractCompressedMetadataPtr();
}


} // namespace nx::recording::helpers
