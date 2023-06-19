// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/streaming/video_data_packet.h>
#include <nx/streaming/av_codec_media_context.h>

namespace nx::media {

NX_VMS_COMMON_API std::vector<uint8_t> buildExtraDataMp4(const QnCompressedVideoData* frame);
NX_VMS_COMMON_API bool isMp4Format(const QnCompressedVideoData* videoData);
NX_VMS_COMMON_API bool isAnnexb(const QnCompressedVideoData* data);
NX_VMS_COMMON_API QnCompressedVideoDataPtr convertStartCodesToSizes(
    const QnCompressedVideoData* frame);

class NX_VMS_COMMON_API AnnexbToMp4
{
public:
    QnCompressedVideoDataPtr process(const QnCompressedVideoData* frame);

private:
    void updateCodecParameters(const QnCompressedVideoData* frame);

private:
    CodecParametersConstPtr m_context;
};

} // namespace nx::media
