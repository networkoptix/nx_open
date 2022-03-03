// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <optional>

#include <camera/camera_plugin_types.h>
#include <nx/sdk/archive/i_codec_info.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/streaming/media_data_packet.h>
#include <utils/media/av_codec_helper.h>
#include <plugins/plugin_tools.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libavformat/avformat.h>
} // extern "C"


namespace nx::utils::media::sdk_support {

struct NX_VMS_COMMON_API CodecInfo
{
    nxcip::CompressionType compressionType = nxcip::CompressionType::AV_CODEC_ID_NONE;
    nxcip::PixelFormat pixelFormat = nxcip::PixelFormat::AV_PIX_FMT_YUV420P;
    nxcip::MediaType mediaType = nxcip::MediaType::AVMEDIA_TYPE_UNKNOWN;
    int width = -1;
    int height = -1;
    int64_t codecTag = -1;
    int64_t bitRate = -1;
    int channels = -1;
    int frameSize = -1;
    int blockAlign = -1;
    int sampleRate = -1;
    nxcip::SampleFormat sampleFormat = nxcip::SampleFormat::AV_SAMPLE_FMT_NONE;
    int bitsPerCodedSample = -1;
    int64_t channelLayout = -1;
    uint8_t extradata[2048];
    int extradataSize = 0;
    int channelNumber = -1;

    CodecInfo();
};

NX_VMS_COMMON_API nxcip::CompressionType toNxCompressionType(AVCodecID codecId);
NX_VMS_COMMON_API AVCodecID toAVCodecId(nxcip::CompressionType codecId);

NX_VMS_COMMON_API nxcip::MediaType toNxMediaType(AVMediaType mediaType);
NX_VMS_COMMON_API AVMediaType toAvMediaType(nxcip::MediaType mediaType);

AVPixelFormat toAVPixelFormat(nxcip::PixelFormat pixelFormat);
nxcip::PixelFormat toNxPixelFormat(AVPixelFormat pixelFormat);

AVSampleFormat toAVSampleFormat(nxcip::SampleFormat sampleFormat);
nxcip::SampleFormat toNxSampleFormat(AVSampleFormat sampleFormat);

NX_VMS_COMMON_API void avCodecParametersFromCodecInfo(const CodecInfo& info, AVCodecParameters* codecParams);
NX_VMS_COMMON_API CodecInfo codecInfoFromAvCodecParameters(const AVCodecParameters* codecParams);
NX_VMS_COMMON_API void setupIoContext(
    AVFormatContext* formatContext,
    void* userCtx,
    int (*read)(void* userCtx, uint8_t* data, int size),
    int (*write)(void* userCtx, uint8_t* data, int size),
    int64_t (*seek)(void* userCtx, int64_t pos, int whence)) noexcept(false);

class ThirdPartyMediaDataPacket:
    public nxcip::MediaDataPacket,
    public nxcip::Encryptable
{
public:
    ThirdPartyMediaDataPacket(
        const QnConstAbstractMediaDataPtr& mediaData,
        std::optional<int> streamIndex);

    // MediaDataPacket
    virtual nxcip::UsecUTCTimestamp timestamp() const override;
    virtual nxcip::DataPacketType type() const override;
    virtual const void* data() const override;
    virtual unsigned int dataSize() const override;
    virtual unsigned int channelNumber() const override;
    virtual nxcip::CompressionType codecType() const override;
    virtual unsigned int flags() const override;
    virtual unsigned int cSeq() const override;
    virtual int addRef() const override;
    virtual int releaseRef() const override;
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceID) override;

    // Encryptable
    virtual const uint8_t* encryptionData() const override;
    virtual int encryptionDataSize() const override;

private:
    nxpt::CommonRefManager m_refManager;
    const QnConstAbstractMediaDataPtr& m_mediaData;
    std::optional<int> m_streamIndex;
    QByteArray m_serializedMetadata;
};

NX_VMS_COMMON_API nx::sdk::Ptr<nxcip::MediaDataPacket> mediaPacketFromFrame(
    const QnConstAbstractMediaDataPtr& mediaData,
    std::optional<int> streamIndex = std::nullopt);

NX_VMS_COMMON_API QnAbstractMediaData::DataType toMediaDataType(nxcip::DataPacketType type);
nxcip::DataPacketType toSdkDataPacketType(QnAbstractMediaData::DataType type);

class NX_VMS_COMMON_API SdkCodecInfo: public nx::sdk::RefCountable<nx::sdk::archive::ICodecInfo>
{
public:
    SdkCodecInfo(const CodecInfo& codecInfo);
    virtual nxcip::CompressionType compressionType() const override;
    virtual nxcip::PixelFormat pixelFormat() const override;
    virtual nxcip::MediaType mediaType() const override;
    virtual int width() const override;
    virtual int height() const override;
    virtual int64_t codecTag() const override;
    virtual int64_t bitRate() const override;
    virtual int channels() const override;
    virtual int frameSize() const override;
    virtual int blockAlign() const override;
    virtual int sampleRate() const override;
    virtual nxcip::SampleFormat sampleFormat() const override;
    virtual int bitsPerCodedSample() const override;
    virtual int64_t channelLayout() const override;
    virtual int extradataSize() const override;
    virtual const uint8_t* extradata() const override;
    virtual int channelNumber() const override;

private:
    CodecInfo m_codecInfo;
};

NX_VMS_COMMON_API nx::utils::media::sdk_support::CodecInfo codecInfo(
    const nx::sdk::archive::ICodecInfo* info);

} // nx::utils::media::sdk_support
