// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <stdint.h>

#include <QtCore/QIODevice>
#include <QtCore/QString>

#include <common/common_globals.h>
#include <core/resource/resource_media_layout.h>
#include <nx/media/video_data_packet.h>

extern "C" {
#include <libavformat/avio.h>
}

namespace nx::transcoding {

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

NX_VMS_COMMON_API bool useMultiThreadEncode(AVCodecID codec, QSize resolution);
QRect roundRect(const QRect& srcRect);
QSize roundSize(const QSize& size);

NX_VMS_COMMON_API AVCodecID findEncoderCodecId(const QString& codecName);

QSize maxResolution(AVCodecID codec);
NX_VMS_COMMON_API QSize normalizeResolution(const QSize& target, const QSize& source);
NX_VMS_COMMON_API QSize adjustCodecRestrictions(AVCodecID codec, const QSize& source);
QSize cropResolution(const QSize& source, const QSize& max);
QSize downscaleByHeight(const QSize& source, int newHeight);
QSize downscaleByWidth(const QSize& source, int newWidth);
QSize findMaxSavedResolution(const QnConstCompressedVideoDataPtr& video);

} // namespace nx::transcoding

/*!
    \note All constants (except \a quality) in this namespace refer to libavcodec CodecContex field names
*/
namespace QnCodecParams
{
    typedef QMap<QByteArray, QVariant> Value;

    static const QByteArray quality( "quality" );
    static const QByteArray qmin( "qmin" );
    static const QByteArray qmax( "qmax" );
    static const QByteArray qscale( "qscale" );
    static const QByteArray global_quality( "global_quality" );
    static const QByteArray gop_size( "gop_size" );
}

/**
 * Selects media stream parameters based on codec, resolution and quality
 */
QnCodecParams::Value NX_VMS_COMMON_API suggestMediaStreamParams(
    AVCodecID codec,
    Qn::StreamQuality quality);

/**
 * Suggest media bitrate based on codec, resolution and quality
 */
int NX_VMS_COMMON_API suggestBitrate(
    AVCodecID codec,
    QSize resolution,
    Qn::StreamQuality quality,
    const char* codecName = nullptr);
