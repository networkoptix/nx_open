// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "transcoding_utils.h"

#include <array>

#include <core/resource/camera_media_stream_info.h>
#include <core/resource/camera_resource.h>
#include <nx/codec/h263/h263_utils.h>
#include <nx/fusion/serialization/json.h>
#include <nx/media/codec_parameters.h>
#include <nx/media/ffmpeg_helper.h>
#include <nx/streaming/abstract_stream_data_provider.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>

namespace nx {
namespace transcoding {

namespace {

constexpr int kWidthAlign = 16;
constexpr int kHeightAlign = 4;

} // namespace

AVCodecID findEncoderCodecId(const QString& codecName)
{
    QString codecLower = codecName.toLower();
    if (codecLower == "h263")
        codecLower = "h263p"; //< force using h263p codec

    const AVCodec* avCodec = avcodec_find_encoder_by_name(codecLower.toUtf8().constData());
    if (avCodec)
        return avCodec->id;

    // Try to check codec substitutes if requested codecs not found.
    static std::map<std::string, std::vector<std::string>> codecSubstitutesMap
    {
        { "h264", {"libopenh264"} },
    };
    auto substitutes = codecSubstitutesMap.find(codecLower.toUtf8().constData());
    if (substitutes == codecSubstitutesMap.end())
        return AV_CODEC_ID_NONE;

    for(auto& substitute: substitutes->second)
    {
        avCodec = avcodec_find_encoder_by_name(substitute.c_str());
        if (avCodec)
            return avCodec->id;
    }
    return AV_CODEC_ID_NONE;
}

QSize downscaleByHeight(const QSize& source, int newHeight)
{
    float ar = source.width() / (float)source.height();
    return QSize(newHeight * ar, newHeight);
}

QSize downscaleByWidth(const QSize& source, int newWidth)
{
    float ar = source.width() / (float)source.height();
    return QSize(newWidth, newWidth / ar);
}

QSize cropResolution(const QSize& source, const QSize& max)
{
    QSize result = source;
    if (result.height() > max.height())
        result = downscaleByHeight(result, max.height());

    if (result.width() > max.width())
        result = downscaleByWidth(result, max.width());

    return result;
}

QSize maxResolution(AVCodecID codec)
{
    using namespace nx::media;
    struct CodecMaxSize
    {
        AVCodecID codec;
        QSize maxSize;
    };
    static const CodecMaxSize maxSizes[] =
    {
        { AV_CODEC_ID_H263P, QSize(h263::kMaxWidth, h263::kMaxHeight)},
        { AV_CODEC_ID_MPEG2VIDEO, QSize(4092, 4092) }, // Temporary limit mpeg2video to simplify code, will fix it in 4.2.
        { AV_CODEC_ID_MJPEG, QSize(2032, 2032)},
    };

    for(auto& size: maxSizes)
    {
        if (size.codec == codec)
            return size.maxSize;
    }
    return QSize(8192 - 16, 8192 - 16);
}

QSize adjustCodecRestrictions(AVCodecID codec, const QSize& source)
{
    QSize result(source);
    QSize maxSize = maxResolution(codec);
    if (source.width() > maxSize.width() || source.height() > maxSize.height())
    {
        result = cropResolution(result, maxSize);
        result.setWidth(qPower2Round(result.width(), kWidthAlign));
        result.setHeight(qPower2Round(result.height(), kHeightAlign));
        NX_DEBUG(NX_SCOPE_TAG, "Codec '%1' does not support resolution %2, downscale to %3",
            avcodec_get_name(codec), source, result);
    }
    switch (codec)
    {
        case AV_CODEC_ID_MPEG2VIDEO:
            // Width or Height are not allowed to be multiples of 4096 (see ffmpeg sources)
            if ((result.width() & 0xFFF) == 0)
                result.setWidth(result.width() - 4);

            if ((result.height() & 0xFFF) == 0)
                result.setHeight(result.height() - 4);

            if (source != result)
            {
                NX_DEBUG(NX_SCOPE_TAG, "mpeg2video does not support resolution %1, change it to %2",
                    source, result);
            }
            return result;

        default:
            return result;
    }
}

QSize normalizeResolution(const QSize& target, const QSize& source)
{
    if (target.width() == 0 && target.height() > 0)
    {
        if (source.isEmpty())
            return QSize();

        QSize result = downscaleByHeight(source, target.height());
        return result;
    }
    return target;
}

QSize findMaxSavedResolution(const QnConstCompressedVideoDataPtr& video)
{
    if (!video->dataProvider)
        return QSize();

    QnResourcePtr res = video->dataProvider->getResource();
    if (!res)
        return QSize();

    const CameraMediaStreams supportedMediaStreams =
        QJson::deserialized<CameraMediaStreams>(
            res->getProperty(ResourcePropertyKey::kMediaStreams).toLatin1());

    QSize result;
    for(const auto& stream: supportedMediaStreams.streams)
    {
        QSize size = stream.getResolution();
        if (size.height() > result.height())
            result = size;
    }
    return result;
}

} // namespace transcoding
} // namespace nx
