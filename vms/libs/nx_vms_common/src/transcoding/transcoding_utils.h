// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>

#include <QtCore/QString>
#include <QtCore/QIODevice>

#include <core/resource/resource_media_layout.h>
#include <nx/streaming/video_data_packet.h>

extern "C" {
#include <libavformat/avio.h>
}

namespace nx {
namespace transcoding {

enum class Error
{
    noError,
    unknown,
    noStreamInfo,
    unknownFormat,
    unableToOpenInput,
    badAlloc,
    noEncoder,
    noDecoder,
    failedToWriteHeader
};

NX_VMS_COMMON_API AVCodecID findEncoderCodecId(const QString& codecName);

QSize maxResolution(AVCodecID codec);
NX_VMS_COMMON_API QSize normalizeResolution(const QSize& target, const QSize& source);
NX_VMS_COMMON_API QSize adjustCodecRestrictions(AVCodecID codec, const QSize& source);
QSize cropResolution(const QSize& source, const QSize& max);
QSize downscaleByHeight(const QSize& source, int newHeight);
QSize downscaleByWidth(const QSize& source, int newWidth);
QSize findMaxSavedResolution(const QnConstCompressedVideoDataPtr& video);

} // namespace transcoding
} // namespace nx
