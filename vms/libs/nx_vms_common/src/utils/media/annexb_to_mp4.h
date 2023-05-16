// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/media/codec_parameters.h>
#include <nx/media/video_data_packet.h>

namespace nx::media {

NX_VMS_COMMON_API bool isMp4Format(const QnCompressedVideoData* videoData);
NX_VMS_COMMON_API bool isAnnexb(const QnCompressedVideoData* videoData);
NX_VMS_COMMON_API QnCompressedVideoDataPtr convertStartCodesToSizes(
    const QnCompressedVideoData* frame);

class NX_VMS_COMMON_API AnnexbToMp4
{
public:
    QnCompressedVideoDataPtr process(const QnCompressedVideoData* frame);

private:
    CodecParametersConstPtr m_context;
};

} // namespace nx::media
