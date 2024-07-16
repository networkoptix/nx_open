// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_sdk_support.h"

#include <nx/media/ffmpeg_helper.h>
#include <nx/sdk/cloud_storage/helpers/algorithm.h>
#include <nx/sdk/cloud_storage/helpers/media_data_packet.h>
#include <nx/utils/log/assert.h>
#include <nx/utils/scope_guard.h>
#include <recording/helpers/recording_context_helpers.h>

namespace nx::utils::media::sdk_support {

nxcip::CompressionType toNxCompressionType(AVCodecID codecId)
{
    switch (codecId)
    {
        case AV_CODEC_ID_MPEG2VIDEO:
            return nxcip::AV_CODEC_ID_MPEG2VIDEO;
        case AV_CODEC_ID_H263:
        case AV_CODEC_ID_H263P:
        case AV_CODEC_ID_H263I:
            return nxcip::AV_CODEC_ID_H263;
        case AV_CODEC_ID_MJPEG:
            return nxcip::AV_CODEC_ID_MJPEG;
        case AV_CODEC_ID_MPEG4:
            return nxcip::AV_CODEC_ID_MPEG4;
        case AV_CODEC_ID_H264:
            return nxcip::AV_CODEC_ID_H264;
        case AV_CODEC_ID_VP8:
            return nxcip::AV_CODEC_ID_VP8;
        case AV_CODEC_ID_VP9:
            return nxcip::AV_CODEC_ID_VP9;
        case AV_CODEC_ID_H265:
            return nxcip::AV_CODEC_ID_H265;
        case  AV_CODEC_ID_THEORA:
            return nxcip::AV_CODEC_ID_THEORA;
        case AV_CODEC_ID_PNG:
            return nxcip::AV_CODEC_ID_PNG;
        case AV_CODEC_ID_GIF:
            return nxcip::AV_CODEC_ID_GIF;
        case AV_CODEC_ID_MP2:
            return nxcip::AV_CODEC_ID_MP2;
        case AV_CODEC_ID_MP3:
            return nxcip::AV_CODEC_ID_MP3;
        case AV_CODEC_ID_AAC:
            return nxcip::AV_CODEC_ID_AAC;
        case AV_CODEC_ID_AC3:
            return nxcip::AV_CODEC_ID_AC3;
        case AV_CODEC_ID_DTS:
            return nxcip::AV_CODEC_ID_DTS;
        case AV_CODEC_ID_PCM_S16LE:
            return nxcip::AV_CODEC_ID_PCM_S16LE;
        case AV_CODEC_ID_PCM_MULAW:
            return nxcip::AV_CODEC_ID_PCM_MULAW;
        case AV_CODEC_ID_VORBIS:
            return nxcip::AV_CODEC_ID_VORBIS;
        case AV_CODEC_ID_TEXT:
        case AV_CODEC_ID_SUBRIP:
            return nxcip::AV_CODEC_ID_TEXT;
        case AV_CODEC_ID_NONE:
            return nxcip::AV_CODEC_ID_NONE;
        case AV_CODEC_ID_BIN_DATA:
            return nxcip::AV_CODEC_ID_BIN_DATA;
        default:
            NX_ASSERT(false);
    }

    return nxcip::AV_CODEC_ID_NONE;
}

AVCodecID toAVCodecId(nxcip::CompressionType codecId)
{
    switch (codecId)
    {
        case nxcip::AV_CODEC_ID_MPEG2VIDEO:
            return AV_CODEC_ID_MPEG2VIDEO;
        case nxcip::AV_CODEC_ID_H263:
            return AV_CODEC_ID_H263P;
        case nxcip::AV_CODEC_ID_MJPEG:
            return AV_CODEC_ID_MJPEG;
        case nxcip::AV_CODEC_ID_MPEG4:
            return AV_CODEC_ID_MPEG4;
        case nxcip::AV_CODEC_ID_H264:
            return AV_CODEC_ID_H264;
        case nxcip::AV_CODEC_ID_VP8:
            return AV_CODEC_ID_VP8;
        case nxcip::AV_CODEC_ID_VP9:
            return AV_CODEC_ID_VP9;
        case nxcip::AV_CODEC_ID_H265:
            return AV_CODEC_ID_H265;
        case nxcip::AV_CODEC_ID_THEORA:
            return AV_CODEC_ID_THEORA;
        case nxcip::AV_CODEC_ID_PNG:
            return AV_CODEC_ID_PNG;
        case nxcip::AV_CODEC_ID_GIF:
            return AV_CODEC_ID_GIF;
        case nxcip::AV_CODEC_ID_MP2:
            return AV_CODEC_ID_MP2;
        case nxcip::AV_CODEC_ID_MP3:
            return AV_CODEC_ID_MP3;
        case nxcip::AV_CODEC_ID_AAC:
            return AV_CODEC_ID_AAC;
        case nxcip::AV_CODEC_ID_AC3:
            return AV_CODEC_ID_AC3;
        case nxcip::AV_CODEC_ID_DTS:
            return AV_CODEC_ID_DTS;
        case nxcip::AV_CODEC_ID_PCM_S16LE:
            return AV_CODEC_ID_PCM_S16LE;
        case nxcip::AV_CODEC_ID_PCM_MULAW:
            return AV_CODEC_ID_PCM_MULAW;
        case nxcip::AV_CODEC_ID_VORBIS:
            return AV_CODEC_ID_VORBIS;
        case nxcip::AV_CODEC_ID_TEXT:
            return AV_CODEC_ID_TEXT;
        case nxcip::AV_CODEC_ID_NONE:
            return AV_CODEC_ID_NONE;
        case nxcip::AV_CODEC_ID_BIN_DATA:
            return AV_CODEC_ID_BIN_DATA;
    }

    NX_ASSERT(false);
    return AV_CODEC_ID_NONE;
}

nxcip::MediaType toNxMediaType(AVMediaType mediaType)
{
    switch (mediaType)
    {
        case AVMEDIA_TYPE_NB: return nxcip::AVMEDIA_TYPE_NB;
        case AVMEDIA_TYPE_DATA: return nxcip::AVMEDIA_TYPE_DATA;
        case AVMEDIA_TYPE_AUDIO: return nxcip::AVMEDIA_TYPE_AUDIO;
        case AVMEDIA_TYPE_VIDEO: return nxcip::AVMEDIA_TYPE_VIDEO;
        case AVMEDIA_TYPE_UNKNOWN: return nxcip::AVMEDIA_TYPE_UNKNOWN;
        case AVMEDIA_TYPE_SUBTITLE: return nxcip::AVMEDIA_TYPE_SUBTITLE;
        case AVMEDIA_TYPE_ATTACHMENT: return nxcip::AVMEDIA_TYPE_ATTACHMENT;
    }

    NX_ASSERT(false);
    return nxcip::AVMEDIA_TYPE_UNKNOWN;
}

AVMediaType toAvMediaType(nxcip::MediaType mediaType)
{
    switch (mediaType)
    {
        case nxcip::AVMEDIA_TYPE_NB: return AVMEDIA_TYPE_NB;
        case nxcip::AVMEDIA_TYPE_DATA: return AVMEDIA_TYPE_DATA;
        case nxcip::AVMEDIA_TYPE_AUDIO: return AVMEDIA_TYPE_AUDIO;
        case nxcip::AVMEDIA_TYPE_VIDEO: return AVMEDIA_TYPE_VIDEO;
        case nxcip::AVMEDIA_TYPE_UNKNOWN: return AVMEDIA_TYPE_UNKNOWN;
        case nxcip::AVMEDIA_TYPE_SUBTITLE: return AVMEDIA_TYPE_SUBTITLE;
        case nxcip::AVMEDIA_TYPE_ATTACHMENT: return AVMEDIA_TYPE_ATTACHMENT;
    }

    NX_ASSERT(false);
    return AVMEDIA_TYPE_UNKNOWN;
}

AVPixelFormat toAVPixelFormat(nxcip::PixelFormat pixelFormat)
{
    switch (pixelFormat)
    {
        case nxcip::PixelFormat::AV_PIX_FMT_BGRA:
            return AV_PIX_FMT_BGRA;
        case nxcip::PixelFormat::AV_PIX_FMT_NV12:
            return AV_PIX_FMT_NV12;
        case nxcip::PixelFormat::AV_PIX_FMT_RGBA:
            return AV_PIX_FMT_RGBA;
        case nxcip::PixelFormat::AV_PIX_FMT_GRAY8:
            return AV_PIX_FMT_GRAY8;
        case nxcip::AV_PIX_FMT_RGB24:
            return AV_PIX_FMT_RGB24;
        case nxcip::AV_PIX_FMT_YUV420P:
            return AV_PIX_FMT_YUV420P;
        case nxcip::AV_PIX_FMT_YUV422P:
            return AV_PIX_FMT_YUV422P;
        case nxcip::AV_PIX_FMT_YUV444P:
            return AV_PIX_FMT_YUV444P;
        case nxcip::AV_PIX_FMT_YUVA420P:
            return AV_PIX_FMT_YUVA420P;
        case nxcip::AV_PIX_FMT_MONOBLACK:
            return AV_PIX_FMT_MONOBLACK;
        case nxcip::AV_PIX_FMT_YUVJ420P:
            return AV_PIX_FMT_YUVJ420P;
        case nxcip::AV_PIX_FMT_NONE:
            return AV_PIX_FMT_NONE;
    }

    NX_ASSERT(false);
    return AV_PIX_FMT_NONE;
}

nxcip::PixelFormat toNxPixelFormat(AVPixelFormat pixelFormat)
{
    switch (pixelFormat)
    {
        case AV_PIX_FMT_BGRA:
            return nxcip::PixelFormat::AV_PIX_FMT_BGRA;
        case AV_PIX_FMT_NV12:
            return nxcip::PixelFormat::AV_PIX_FMT_NV12;
        case AV_PIX_FMT_RGBA:
            return nxcip::PixelFormat::AV_PIX_FMT_RGBA;
        case AV_PIX_FMT_GRAY8:
            return nxcip::PixelFormat::AV_PIX_FMT_GRAY8;
        case AV_PIX_FMT_RGB24:
            return nxcip::PixelFormat::AV_PIX_FMT_RGB24;
        case AV_PIX_FMT_YUV420P:
            return nxcip::PixelFormat::AV_PIX_FMT_YUV420P;
        case AV_PIX_FMT_YUV422P:
            return nxcip::PixelFormat::AV_PIX_FMT_YUV422P;
        case AV_PIX_FMT_YUV444P:
            return nxcip::PixelFormat::AV_PIX_FMT_YUV444P;
        case AV_PIX_FMT_YUVA420P:
            return nxcip::PixelFormat::AV_PIX_FMT_YUVA420P;
        case AV_PIX_FMT_MONOBLACK:
            return nxcip::PixelFormat::AV_PIX_FMT_MONOBLACK;
        case AV_PIX_FMT_YUVJ420P:
            return nxcip::PixelFormat::AV_PIX_FMT_YUVJ420P;
        case AV_PIX_FMT_NONE:
            return nxcip::PixelFormat::AV_PIX_FMT_NONE;
        default:
            NX_ASSERT(false);
    }

    return nxcip::PixelFormat::AV_PIX_FMT_NONE;
}

AVSampleFormat toAVSampleFormat(nxcip::SampleFormat sampleFormat)
{
    switch (sampleFormat)
    {
        case nxcip::AV_SAMPLE_FMT_DBL:
            return AV_SAMPLE_FMT_DBL;
        case nxcip::AV_SAMPLE_FMT_DBLP:
            return AV_SAMPLE_FMT_DBLP;
        case nxcip::AV_SAMPLE_FMT_FLT:
            return AV_SAMPLE_FMT_FLT;
        case nxcip::AV_SAMPLE_FMT_FLTP:
            return AV_SAMPLE_FMT_FLTP;
        case nxcip::AV_SAMPLE_FMT_NB:
            return AV_SAMPLE_FMT_NB;
        case nxcip::AV_SAMPLE_FMT_NONE:
            return AV_SAMPLE_FMT_NONE;
        case nxcip::AV_SAMPLE_FMT_S16:
            return AV_SAMPLE_FMT_S16;
        case nxcip::AV_SAMPLE_FMT_S16P:
            return AV_SAMPLE_FMT_S16P;
        case nxcip::AV_SAMPLE_FMT_S32:
            return AV_SAMPLE_FMT_S32;
        case nxcip::AV_SAMPLE_FMT_S32P:
            return AV_SAMPLE_FMT_S32P;
        case nxcip::AV_SAMPLE_FMT_U8:
            return AV_SAMPLE_FMT_U8;
        case nxcip::AV_SAMPLE_FMT_U8P:
            return AV_SAMPLE_FMT_U8P;
        case nxcip::AV_SAMPLE_FMT_S64:
            return AV_SAMPLE_FMT_S64;
        case nxcip::AV_SAMPLE_FMT_S64P:
            return AV_SAMPLE_FMT_S64P;
    }

    NX_ASSERT(false);
    return AV_SAMPLE_FMT_NONE;
}

nxcip::SampleFormat toNxSampleFormat(AVSampleFormat sampleFormat)
{
    switch (sampleFormat)
    {
        case AV_SAMPLE_FMT_DBL:
            return nxcip::AV_SAMPLE_FMT_DBL;
        case AV_SAMPLE_FMT_DBLP:
            return nxcip::AV_SAMPLE_FMT_DBLP;
        case AV_SAMPLE_FMT_FLT:
            return nxcip::AV_SAMPLE_FMT_FLT;
        case AV_SAMPLE_FMT_FLTP:
            return nxcip::AV_SAMPLE_FMT_FLTP;
        case AV_SAMPLE_FMT_NB:
            return nxcip::AV_SAMPLE_FMT_NB;
        case AV_SAMPLE_FMT_NONE:
            return nxcip::AV_SAMPLE_FMT_NONE;
        case AV_SAMPLE_FMT_S16:
            return nxcip::AV_SAMPLE_FMT_S16;
        case AV_SAMPLE_FMT_S16P:
            return nxcip::AV_SAMPLE_FMT_S16P;
        case AV_SAMPLE_FMT_S32:
            return nxcip::AV_SAMPLE_FMT_S32;
        case AV_SAMPLE_FMT_S32P:
            return nxcip::AV_SAMPLE_FMT_S32P;
        case AV_SAMPLE_FMT_U8:
            return nxcip::AV_SAMPLE_FMT_U8;
        case AV_SAMPLE_FMT_U8P:
            return nxcip::AV_SAMPLE_FMT_U8P;
        case AV_SAMPLE_FMT_S64:
            return nxcip::AV_SAMPLE_FMT_S64;
        case AV_SAMPLE_FMT_S64P:
            return nxcip::AV_SAMPLE_FMT_S64P;
    }

    NX_ASSERT(false);
    return nxcip::AV_SAMPLE_FMT_NONE;
}

void avCodecParametersFromCodecInfo(
    const nx::sdk::cloud_storage::CodecInfoData& info, AVCodecParameters* codecParams)
{
#define SET_IF_DEFINED_INT(dst, src) \
        do \
        { \
            if (src >= 0) \
                dst = src; \
        } while (0)

    codecParams->codec_id = toAVCodecId(info.compressionType);
    SET_IF_DEFINED_INT(codecParams->codec_type, toAvMediaType(info.mediaType));
    if (codecParams->codec_type == AVMEDIA_TYPE_VIDEO)
        SET_IF_DEFINED_INT(codecParams->format, toAVPixelFormat(info.pixelFormat));
    else
        SET_IF_DEFINED_INT(codecParams->format, toAVSampleFormat(info.sampleFormat));

    SET_IF_DEFINED_INT(codecParams->width, info.width);
    SET_IF_DEFINED_INT(codecParams->height, info.height);
    SET_IF_DEFINED_INT(codecParams->codec_tag, info.codecTag);
    SET_IF_DEFINED_INT(codecParams->bit_rate, info.bitRate);
    SET_IF_DEFINED_INT(codecParams->ch_layout.nb_channels, info.channels);
    SET_IF_DEFINED_INT(codecParams->frame_size, info.frameSize);
    SET_IF_DEFINED_INT(codecParams->sample_rate, info.sampleRate);
    SET_IF_DEFINED_INT(codecParams->bits_per_coded_sample, info.bitsPerCodedSample);
    SET_IF_DEFINED_INT(codecParams->ch_layout.u.mask, info.channelLayout);

    const auto extradata = nx::sdk::cloud_storage::fromBase64(info.extradataBase64);
    QnFfmpegHelper::copyAvCodecContextField(
        (void**) &codecParams->extradata, extradata.data(), extradata.size());

    codecParams->extradata_size = extradata.size();

#undef SET_IF_DEFINED_INT
#undef SET_IF_DEFINED_RATIONAL
}

nx::sdk::cloud_storage::CodecInfoData codecInfoFromAvCodecParameters(
    const AVCodecParameters* codecParams)
{
    nx::sdk::cloud_storage::CodecInfoData result;
    result.mediaType = toNxMediaType(codecParams->codec_type);
    result.codecTag = codecParams->codec_tag;
    result.compressionType = toNxCompressionType(codecParams->codec_id);
    result.bitRate = codecParams->bit_rate;
    result.height = codecParams->height;
    result.width = codecParams->width;
    if (codecParams->codec_type == AVMEDIA_TYPE_VIDEO)
        result.pixelFormat = toNxPixelFormat((AVPixelFormat)codecParams->format);

    if (codecParams->codec_type == AVMEDIA_TYPE_AUDIO)
        result.sampleFormat = toNxSampleFormat((AVSampleFormat)codecParams->format);

    result.frameSize = codecParams->frame_size;
    result.sampleRate = codecParams->sample_rate;
    result.bitsPerCodedSample = codecParams->bits_per_coded_sample;
    result.channels = codecParams->ch_layout.nb_channels;
    result.channelLayout = codecParams->ch_layout.u.mask;
    result.blockAlign = codecParams->block_align;
    result.extradataBase64 = nx::sdk::cloud_storage::toBase64(
        codecParams->extradata, codecParams->extradata_size);

    return result;
}

void setupIoContext(
    AVFormatContext* formatContext,
    void* userCtx,
    int (*read)(void* userCtx, uint8_t* data, int size),
    int (*write)(void* userCtx, const uint8_t* data, int size),
    int64_t (*seek)(void* userCtx, int64_t pos, int whence)) noexcept(false)
{
    constexpr int kBlockSize = 1024 * 1024;
    uint8_t* ioBuffer;
    ioBuffer = static_cast<uint8_t*>(av_malloc(kBlockSize));
    formatContext->pb = avio_alloc_context(ioBuffer, kBlockSize, 1, userCtx, read, write, seek);
    if (!formatContext->pb)
        throw std::runtime_error("Failed to initialize ffmpeg: avio_alloc_context");
}

nx::sdk::Ptr<nx::sdk::cloud_storage::IMediaDataPacket> mediaPacketFromFrame(
    const QnConstAbstractMediaDataPtr& mediaData,
    int streamIndex)
{
    nx::sdk::cloud_storage::MediaPacketData data;
    data.channelNumber = mediaData->channelNumber;
    data.compressionType = toNxCompressionType(mediaData->compressionType);
    if (auto metadata = std::dynamic_pointer_cast<const QnAbstractCompressedMetadata>(mediaData))
    {
        const auto serializedMetadata = nx::recording::helpers::serializeMetadataPacket(metadata);
        data.data.assign(
            serializedMetadata.constData(), serializedMetadata.constData() + serializedMetadata.size());
        data.dataSize = serializedMetadata.size()   ;
    }
    else
    {
        data.data.assign(mediaData->data(), mediaData->data() + mediaData->dataSize());
        data.dataSize = mediaData->dataSize();
    }

    data.encryptionData = mediaData->encryptionData;
    data.isKeyFrame = mediaData->flags & QnAbstractMediaData::MediaFlags_AVKey;
    data.timestampUs = mediaData->timestamp;
    switch (mediaData->dataType)
    {
        case QnAbstractMediaData::DataType::AUDIO:
            data.type = nx::sdk::cloud_storage::IMediaDataPacket::Type::audio;
            break;
        case QnAbstractMediaData::DataType::VIDEO:
            data.type = nx::sdk::cloud_storage::IMediaDataPacket::Type::video;
            break;
        case QnAbstractMediaData::DataType::GENERIC_METADATA:
            data.type = nx::sdk::cloud_storage::IMediaDataPacket::Type::metadata;
            break;
        default:
            data.type = nx::sdk::cloud_storage::IMediaDataPacket::Type::unknown;
            break;
    }

    return nx::sdk::makePtr<nx::sdk::cloud_storage::MediaDataPacket>(data);
}

QnAbstractMediaData::DataType toMediaDataType(nxcip::DataPacketType type)
{
    switch (type)
    {
        case nxcip::DataPacketType::dptAudio:
            return QnAbstractMediaData::AUDIO;
        case nxcip::DataPacketType::dptEmpty:
            return QnAbstractMediaData::EMPTY_DATA;
        case nxcip::DataPacketType::dptData:
            return QnAbstractMediaData::GENERIC_METADATA;
        case nxcip::DataPacketType::dptUnknown:
            return QnAbstractMediaData::UNKNOWN;
        case nxcip::DataPacketType::dptVideo:
            return QnAbstractMediaData::VIDEO;
    }

    NX_ASSERT(false);
    return QnAbstractMediaData::UNKNOWN;
}

nxcip::DataPacketType toSdkDataPacketType(QnAbstractMediaData::DataType type)
{
    switch (type)
    {
        case QnAbstractMediaData::AUDIO:
            return nxcip::DataPacketType::dptAudio;
        case QnAbstractMediaData::CONTAINER:
        case QnAbstractMediaData::UNKNOWN:
            return nxcip::dptUnknown;
        case QnAbstractMediaData::EMPTY_DATA:
            return nxcip::dptEmpty;
        case QnAbstractMediaData::GENERIC_METADATA:
            return nxcip::dptData;
        case QnAbstractMediaData::VIDEO:
            return nxcip::dptVideo;
    }

    NX_ASSERT(false);
    return nxcip::dptUnknown;
}

} // nx::utils::media::sdk_support
