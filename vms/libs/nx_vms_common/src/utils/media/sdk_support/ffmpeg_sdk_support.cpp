// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "ffmpeg_sdk_support.h"

#include <utils/media/ffmpeg_helper.h>
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

void avCodecParametersFromCodecInfo(const CodecInfo& info, AVCodecParameters* codecParams)
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
    SET_IF_DEFINED_INT(codecParams->channels, info.channels);
    SET_IF_DEFINED_INT(codecParams->frame_size, info.frameSize);
    SET_IF_DEFINED_INT(codecParams->sample_rate, info.sampleRate);
    SET_IF_DEFINED_INT(codecParams->bits_per_coded_sample, info.bitsPerCodedSample);
    SET_IF_DEFINED_INT(codecParams->channel_layout, info.channelLayout);

    QnFfmpegHelper::copyAvCodecContextField(
        (void**) &codecParams->extradata, info.extradata, info.extradataSize);
    codecParams->extradata_size = info.extradataSize;

#undef SET_IF_DEFINED_INT
#undef SET_IF_DEFINED_RATIONAL
}

CodecInfo::CodecInfo()
{
    memset(extradata, 0, sizeof(extradata));
}

CodecInfo codecInfoFromAvCodecParameters(const AVCodecParameters* codecParams)
{
    CodecInfo result;
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
    result.channels = codecParams->channels;
    result.channelLayout = codecParams->channel_layout;
    result.blockAlign = codecParams->block_align;

    NX_ASSERT(codecParams->extradata_size <= (int) sizeof(result.extradata));
    result.extradataSize = std::min<int>(codecParams->extradata_size, sizeof(result.extradata));
    memcpy(result.extradata, codecParams->extradata, result.extradataSize);

    return result;
}

void setupIoContext(
    AVFormatContext* formatContext,
    void* userCtx,
    int (*read)(void* userCtx, uint8_t* data, int size),
    int (*write)(void* userCtx, uint8_t* data, int size),
    int64_t (*seek)(void* userCtx, int64_t pos, int whence)) noexcept(false)
{
    constexpr int kBlockSize = 1024 * 1024;
    uint8_t* ioBuffer;
    ioBuffer = static_cast<uint8_t*>(av_malloc(kBlockSize));
    formatContext->pb = avio_alloc_context(ioBuffer, kBlockSize, 1, userCtx, read, write, seek);
    if (!formatContext->pb)
        throw std::runtime_error("Failed to initialize ffmpeg: avio_alloc_context");
}


ThirdPartyMediaDataPacket::ThirdPartyMediaDataPacket(
    const QnConstAbstractMediaDataPtr& mediaData,
    std::optional<int> streamIndex)
    :
    m_refManager(this),
    m_mediaData(mediaData),
    m_streamIndex(streamIndex)
{
    if (auto metadata = std::dynamic_pointer_cast<const QnAbstractCompressedMetadata>(mediaData))
        m_serializedMetadata = nx::recording::helpers::serializeMetadataPacket(metadata);
}

nxcip::UsecUTCTimestamp ThirdPartyMediaDataPacket::timestamp() const
{
    return (nxcip::UsecUTCTimestamp) m_mediaData->timestamp;
}

nxcip::DataPacketType ThirdPartyMediaDataPacket::type() const
{
    return toSdkDataPacketType(m_mediaData->dataType);
}

const void* ThirdPartyMediaDataPacket::data() const
{
    return m_serializedMetadata.isEmpty() ? m_mediaData->data() : m_serializedMetadata.constData();
}

unsigned int ThirdPartyMediaDataPacket::dataSize() const
{
    return m_serializedMetadata.isEmpty()
        ? (unsigned int) m_mediaData->dataSize()
        : (unsigned int) m_serializedMetadata.size();
}

unsigned int ThirdPartyMediaDataPacket::channelNumber() const
{
    return m_streamIndex ? *m_streamIndex : m_mediaData->channelNumber;
}

nxcip::CompressionType ThirdPartyMediaDataPacket::codecType() const
{
    return toNxCompressionType(m_mediaData->compressionType);
}

unsigned int ThirdPartyMediaDataPacket::flags() const
{
    unsigned int result = 0;
    if ((m_mediaData->flags & QnAbstractMediaData::MediaFlags_AVKey)
        && m_mediaData->dataType != QnAbstractMediaData::GENERIC_METADATA)
    {
        result |= nxcip::MediaDataPacket::fKeyPacket;
    }

    return result;
}

unsigned int ThirdPartyMediaDataPacket::cSeq() const
{
    return 0;
}

int ThirdPartyMediaDataPacket::addRef() const { return m_refManager.addRef(); }

int ThirdPartyMediaDataPacket::releaseRef() const { return m_refManager.releaseRef(); }

void* ThirdPartyMediaDataPacket::queryInterface(const nxpl::NX_GUID& interfaceID)
{
    if (memcmp(interfaceID.bytes, nxcip::IID_MediaDataPacket.bytes, sizeof(interfaceID.bytes)) == 0)
    {
        addRef();
        return dynamic_cast<nxcip::MediaDataPacket*>(this);
    }

    if (memcmp(interfaceID.bytes, nxcip::IID_Encryptable.bytes, sizeof(interfaceID.bytes)) == 0)
    {
        addRef();
        return dynamic_cast<nxcip::Encryptable*>(this);
    }

    return nullptr;
}

const char* ThirdPartyMediaDataPacket::encryptionData() const
{
    return (const char*) m_mediaData->encryptionData.data();
}

int ThirdPartyMediaDataPacket::encryptionDataSize() const
{
    return m_mediaData->encryptionData.size();
}

nx::sdk::Ptr<nxcip::MediaDataPacket> mediaPacketFromFrame(
    const QnConstAbstractMediaDataPtr& mediaData,
    std::optional<int> streamIndex)
{
    nx::sdk::Ptr<nxcip::MediaDataPacket> result = nx::sdk::toPtr(
        new ThirdPartyMediaDataPacket(mediaData, streamIndex));

    return result;
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

SdkCodecInfo::SdkCodecInfo(const CodecInfo& codecInfo) : m_codecInfo(codecInfo)
{
}

nxcip::CompressionType SdkCodecInfo::compressionType() const
{
    return m_codecInfo.compressionType;
}

nxcip::PixelFormat SdkCodecInfo::pixelFormat() const
{
    return m_codecInfo.pixelFormat;
}

nxcip::MediaType SdkCodecInfo::mediaType() const
{
    return m_codecInfo.mediaType;
}

int SdkCodecInfo::width() const
{
    return m_codecInfo.width;
}

int SdkCodecInfo::height() const
{
    return m_codecInfo.height;
}

int64_t SdkCodecInfo::codecTag() const
{
    return m_codecInfo.codecTag;
}

int64_t SdkCodecInfo::bitRate() const
{
    return m_codecInfo.bitRate;
}

int SdkCodecInfo::channels() const
{
    return m_codecInfo.channels;
}

int SdkCodecInfo::frameSize() const
{
    return m_codecInfo.frameSize;
}

int SdkCodecInfo::blockAlign() const
{
    return m_codecInfo.blockAlign;
}

int SdkCodecInfo::sampleRate() const
{
    return m_codecInfo.sampleRate;
}

nxcip::SampleFormat SdkCodecInfo::sampleFormat() const
{
    return m_codecInfo.sampleFormat;
}

int SdkCodecInfo::bitsPerCodedSample() const
{
    return m_codecInfo.bitsPerCodedSample;
}

int64_t SdkCodecInfo::channelLayout() const
{
    return m_codecInfo.channelLayout;
}

int SdkCodecInfo::extradataSize() const
{
    return m_codecInfo.extradataSize;
}

const uint8_t* SdkCodecInfo::extradata() const
{
    return m_codecInfo.extradata;
}

int SdkCodecInfo::channelNumber() const
{
    return m_codecInfo.channelNumber;
}

CodecInfo codecInfo(const nx::sdk::archive::ICodecInfo* info)
{
    sdk_support::CodecInfo result;

    result.compressionType = info->compressionType();
    result.pixelFormat = info->pixelFormat();
    result.mediaType = info->mediaType();
    result.width = info->width();
    result.height = info->height();
    result.codecTag = info->codecTag();
    result.bitRate = info->bitRate();
    result.channels = info->channels();
    result.frameSize = info->frameSize();
    result.blockAlign = info->blockAlign();
    result.sampleRate = info->sampleRate();
    result.sampleFormat = info->sampleFormat();
    result.bitsPerCodedSample = info->bitsPerCodedSample();
    result.channelLayout = info->channelLayout();
    result.extradataSize = info->extradataSize();
    if (result.extradataSize > 0)
        memcpy(result.extradata, info->extradata(), result.extradataSize);
    result.channelNumber = info->channelNumber();
    return result;
}

} // nx::utils::media::sdk_support
