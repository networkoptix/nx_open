// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>
#include <QtCore/QSize>
#include <QtCore/QString>
#include <nx/streaming/video_data_packet.h>
#include <utils/media/hevc/sequence_parameter_set.h>

/** todo: move it to nx_media */

namespace nx::media {

NX_VMS_COMMON_API bool extractSps(
    const QnCompressedVideoData* videoData, hevc::SequenceParameterSet& sps);

NX_VMS_COMMON_API QSize getFrameSize(const QnCompressedVideoData* frame);
NX_VMS_COMMON_API double getDefaultSampleAspectRatio(const QSize& srcSize);
NX_VMS_COMMON_API bool fillExtraData(
    const QnCompressedVideoData* video, uint8_t** outExtradata, int* outSize);

// Build video header(sps, pps, vps(in hevc case)) in ffmpeg extra data format.
NX_VMS_COMMON_API std::vector<uint8_t> buildExtraDataAnnexB(const QnConstCompressedVideoDataPtr& frame);

NX_VMS_COMMON_API QString fromVideoCodectoMimeType(AVCodecID codecId);
NX_VMS_COMMON_API AVCodecID fromMimeTypeToVideoCodec(const std::string& mime);

} // namespace nx::media

