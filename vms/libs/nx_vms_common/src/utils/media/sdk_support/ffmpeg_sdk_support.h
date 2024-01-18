// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <camera/camera_plugin_types.h>
#include <nx/media/av_codec_helper.h>
#include <nx/media/media_data_packet.h>
#include <nx/sdk/cloud_storage/helpers/data.h>
#include <nx/sdk/cloud_storage/i_codec_info.h>
#include <nx/sdk/helpers/ref_countable.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
} // extern "C"

namespace nx::utils::media::sdk_support {

NX_VMS_COMMON_API nxcip::CompressionType toNxCompressionType(AVCodecID codecId);
NX_VMS_COMMON_API AVCodecID toAVCodecId(nxcip::CompressionType codecId);

NX_VMS_COMMON_API nxcip::MediaType toNxMediaType(AVMediaType mediaType);
NX_VMS_COMMON_API AVMediaType toAvMediaType(nxcip::MediaType mediaType);

AVPixelFormat toAVPixelFormat(nxcip::PixelFormat pixelFormat);
nxcip::PixelFormat toNxPixelFormat(AVPixelFormat pixelFormat);

AVSampleFormat toAVSampleFormat(nxcip::SampleFormat sampleFormat);
nxcip::SampleFormat toNxSampleFormat(AVSampleFormat sampleFormat);

NX_VMS_COMMON_API void avCodecParametersFromCodecInfo(
    const nx::sdk::cloud_storage::CodecInfoData& info, AVCodecParameters* codecParams);

NX_VMS_COMMON_API nx::sdk::cloud_storage::CodecInfoData codecInfoFromAvCodecParameters(
    const AVCodecParameters* codecParams);

NX_VMS_COMMON_API void setupIoContext(
    AVFormatContext* formatContext,
    void* userCtx,
    int (*read)(void* userCtx, uint8_t* data, int size),
    int (*write)(void* userCtx, uint8_t* data, int size),
    int64_t (*seek)(void* userCtx, int64_t pos, int whence)) noexcept(false);

NX_VMS_COMMON_API nx::sdk::Ptr<nx::sdk::cloud_storage::IMediaDataPacket> mediaPacketFromFrame(
    const QnConstAbstractMediaDataPtr& mediaData,
   int streamIndex);

NX_VMS_COMMON_API QnAbstractMediaData::DataType toMediaDataType(nxcip::DataPacketType type);
nxcip::DataPacketType toSdkDataPacketType(QnAbstractMediaData::DataType type);

} // nx::utils::media::sdk_support
