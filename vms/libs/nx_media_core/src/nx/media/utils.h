// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QSize>
#include <QtCore/QString>

#include <nx/codec/hevc/sequence_parameter_set.h>
#include <nx/media/video_data_packet.h>

namespace nx::media {

NX_MEDIA_CORE_API bool extractSps(
    const QnCompressedVideoData* videoData, hevc::SequenceParameterSet& sps);

NX_MEDIA_CORE_API QSize getFrameSize(const QnCompressedVideoData* frame);
NX_MEDIA_CORE_API double getDefaultSampleAspectRatio(const QSize& srcSize);
NX_MEDIA_CORE_API bool fillExtraDataAnnexB(
    const QnCompressedVideoData* video, uint8_t** outExtradata, int* outSize);

// Build video header(sps, pps, vps(in hevc case)) in ffmpeg extra data format.
NX_MEDIA_CORE_API std::vector<uint8_t> buildExtraDataAnnexB(const QnConstCompressedVideoDataPtr& frame);

NX_MEDIA_CORE_API QString fromVideoCodectoMimeType(AVCodecID codecId);
NX_MEDIA_CORE_API AVCodecID fromMimeTypeToVideoCodec(const std::string& mime);

} // namespace nx::media
