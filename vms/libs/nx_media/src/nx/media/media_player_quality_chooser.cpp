// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_player_quality_chooser.h"

#include <QtCore/QSize>

#include <core/resource/camera_history.h>
#include <core/resource/camera_media_stream_info.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/media/av_codec_helper.h>
#include <nx/reflect/string_conversion.h>
#include <nx/utils/log/log.h>
#include <nx/utils/math/math.h>
#include <nx/vms/common/system_context.h>

#include "media_player.h" //< For enum VideoQuality.
#include "video_decoder_registry.h"

namespace nx {
namespace media {
namespace media_player_quality_chooser {

namespace {

static const nx::log::Tag kLogTag(QString("MediaPlayerQualityChooser"));
static const int kFallbackLowQualityLines = 360;
static const int kHighQualityFallbackThreshold = 560;
static const QSize kMaxTranscodingResolution(1920, 1080);
static const QSize kDefaultAspect(16, 9);
static const int kWidthRoundingFactor = 16;
static const int kHeightRoundingFactor = 4;

static QString avCodecIdToString(AVCodecID avCodecId)
{
    // NOTE: For AV_CODEC_ID_NONE, QnAvCodecHelper::codecIdToString() returns an empty string, and
    // for unregistered values it returns null string.

    if (avCodecId == AV_CODEC_ID_NONE)
        return "NONE";

    const QString s = QnAvCodecHelper::codecIdToString(avCodecId);

    if (s.isEmpty())
        return QString::number((int) avCodecId);

    return s;
}

struct Streams
{
    QSize highResolution;
    AVCodecID highCodec = AV_CODEC_ID_NONE;
    QSize lowResolution;
    AVCodecID lowCodec = AV_CODEC_ID_NONE;

    QString toString() const
    {
        return QString("Streams{")
            + "highResolution " + nx::toString(highResolution)
            + ", highCodec " + avCodecIdToString(highCodec)
            + ", lowResolution " + nx::toString(lowResolution)
            + ", lowCodec " + avCodecIdToString(lowCodec)
            + "}";
    }
};

/**
 * Obtain Low and High stream codec and resolution.
 *
 * If a particular stream is missing, its parameters are assigned default values.
 */
static Streams findCameraStreams(const QnVirtualCameraResourcePtr& camera)
{
    Streams streams;

    if (camera->mediaStreams().streams.empty())
    {
        NX_DEBUG(kLogTag, "No streams found on camera.");
        return streams;
    }

    for (const auto& stream: camera->mediaStreams().streams)
    {
        if (stream.getEncoderIndex() == nx::vms::api::StreamIndex::primary) //< High
        {
            streams.highCodec = (AVCodecID) stream.codec;
            streams.highResolution = stream.getResolution();
        }
        else if (stream.getEncoderIndex() == nx::vms::api::StreamIndex::secondary) //< Low
        {
            streams.lowCodec = (AVCodecID) stream.codec;
            streams.lowResolution = stream.getResolution();
        }
        else
        {
            NX_DEBUG(kLogTag, "Unrecognized stream index %1", stream.getEncoderIndex());
        }
    }

    NX_DEBUG(kLogTag, streams.toString());
    return streams;
}

static QSize limitResolution(const QSize& desiredResolution, const QSize& limit)
{
    if (desiredResolution.isEmpty() || limit.isEmpty())
        return desiredResolution;

    QSize result = desiredResolution;
    const float aspectRatio = result.width() / (float) result.height();

    if (result.width() > limit.width())
    {
        result.setHeight(qPower2Round(floor(0.5 + limit.width() / aspectRatio), kHeightRoundingFactor));
        result.setWidth(limit.width());
    }

    if (result.height() > limit.height())
    {
        result.setWidth(qPower2Round(floor(0.5 + limit.height() * aspectRatio), kWidthRoundingFactor));
        result.setHeight(limit.height());
    }

    if (result.width() > limit.width())
        result.setWidth(limit.width());
    if (result.height() > limit.height())
        result.setHeight(limit.height());

    return result;
}

static QSize resolutionWithHeightAndAspect(int height, const QSize& aspect)
{
    static const int kWidthRoundingFactor = 16; //< Used for rounding width up.

    const int desiredWidth = (height * aspect.width()) / aspect.height();
    const int roundedWidth = (desiredWidth + (kWidthRoundingFactor - 1))
        / kWidthRoundingFactor * kWidthRoundingFactor;
    const int newHeight = (roundedWidth * aspect.height()) / aspect.width();
    return QSize(roundedWidth, newHeight);
}

static QSize transcodingResolution(
    const QSize& lowResolution,
    const QSize& highResolution,
    int videoQuality,
    AVCodecID transcodingCodec)
{
    // Here videoQuality should not be a special value.
    NX_ASSERT(videoQuality != Player::HighVideoQuality);
    NX_ASSERT(videoQuality != Player::LowVideoQuality);

    QSize aspect = highResolution;
    if (aspect.isEmpty()) //< High stream resolution is unknown.
        aspect = lowResolution;
    if (aspect.isEmpty()) //< Both Low and High stream resolutions are unknown.
        aspect = kDefaultAspect;

    const QSize& desiredResolution = resolutionWithHeightAndAspect(videoQuality, aspect);

    const QSize& maxResolution = VideoDecoderRegistry::instance()->maxResolution(transcodingCodec);

    const QSize& result = limitResolution(desiredResolution, maxResolution);

    if (result == desiredResolution)
    {
        NX_DEBUG(kLogTag,
            nx::format("Custom resolution of %1p requested; desired resolution is %2 x %3:").args(
                videoQuality, desiredResolution.width(), desiredResolution.height()));
    }
    else
    {
        NX_DEBUG(kLogTag,
            nx::format("Custom resolution of %1p requested; desired resolution is %2 x %3, "
                "limited to %4 x %5:").args(
                videoQuality, desiredResolution.width(), desiredResolution.height(),
                result.width(), result.height()));
    }

    return result;
}

/**
 * @return Transcoding resolution, or invalid (default) QSize if transcoding is not possible.
 */
static Result applyTranscodingIfPossible(const QSize& desiredResolution, const Params& input)
{
    if (const Player::TranscodingSupportStatus status =
        transcodingSupportStatus(input.camera, input.positionMs, input.liveMode);
        status != Player::TranscodingSupported)
    {
        NX_DEBUG(kLogTag,
            "Transcoding is not supported for the camera (TranscodingSupportStatus %1).", status);
        return {};
    }

    QSize resolution = limitResolution(desiredResolution, kMaxTranscodingResolution);

    if (!VideoDecoderRegistry::instance()->hasCompatibleDecoder(
        input.transcodingCodec,
        resolution,
        input.allowOverlay,
        input.allowHardwareAcceleration,
        *input.currentDecoders))
    {
        NX_DEBUG(kLogTag, nx::format("Transcoding to %1 x %2 not supported.").args(
            resolution.width(), resolution.height()));
        return {};
    }

    NX_DEBUG(kLogTag, nx::format("Set transcoding to %1 x %2.").args(
        resolution.width(), resolution.height()));
    return {Player::CustomVideoQuality, resolution};
}

static Result chooseHighStreamIfPossible(
    AVCodecID highCodec, const QSize& highResolution, const Params& input)
{
    if (highCodec == AV_CODEC_ID_NONE || !highResolution.isValid()) //< High stream doesn't exist.
    {
        NX_DEBUG(kLogTag, "High stream requested but missing.");
        return {};
    }

    if (input.camera->getVideoLayout()->channelCount() > 1) //< Panoramic camera.
    {
        const QSize& maxResolution =
            VideoDecoderRegistry::instance()->maxResolution(input.transcodingCodec);

        if (maxResolution.isEmpty())
        {
            NX_DEBUG(kLogTag,
                "Panoramic camera: Resolution is not limited, return source resolution %1 x %2:",
                highResolution.width(), highResolution.height());
            return {Player::HighVideoQuality, highResolution};
        }

        const QSize& resolution = limitResolution(highResolution, maxResolution);

        NX_DEBUG(kLogTag,
            nx::format("Panoramic camera: High stream requested => Attempt transcoding to %2 x %3:").args(
                resolution.width(), resolution.height()));

        return applyTranscodingIfPossible(resolution, input);
    }

    if (VideoDecoderRegistry::instance()->hasCompatibleDecoder(
        highCodec, highResolution, input.allowOverlay, input.allowHardwareAcceleration, *input.currentDecoders))
    {
        return {Player::HighVideoQuality, highResolution};
    }

    NX_DEBUG(kLogTag, "High stream requested but compatible decoder is missing.");

    return applyTranscodingIfPossible(highResolution, input);
}

static Result choosePreferredQuality(
    const Streams& streams,
    const int videoQuality,
    const Params& input)
{
    const Result lowQuality = {Player::LowVideoQuality, streams.lowResolution};

    const bool highStreamRequested = videoQuality == Player::HighVideoQuality
        || (streams.highCodec != AV_CODEC_ID_NONE && streams.highResolution.isValid()
            && videoQuality == streams.highResolution.height());

    const bool lowStreamRequested = videoQuality == Player::LowVideoQuality
        || (streams.lowCodec != AV_CODEC_ID_NONE && streams.lowResolution.isValid()
            && videoQuality == streams.lowResolution.height());

    if (videoQuality == Player::LowIframesOnlyVideoQuality)
    {
        if (lowQuality.isValid())
            return {Player::LowIframesOnlyVideoQuality, streams.lowResolution};

        // Player::LowIframesOnlyVideoQuality is used when extremely low CPU and network usage
        // is required and other quality options are not acceptable. So we don't try another
        // options in this case.
        return {};
    }

    if (highStreamRequested)
    {
        const Result result = chooseHighStreamIfPossible(
            streams.highCodec, streams.highResolution, input);
        if (!result.isValid())
            return lowQuality;
        return result;
    }

    if (lowStreamRequested)
        return lowQuality;

    if (videoQuality > Player::CustomVideoQuality) //< Custom lines count requested.
    {
        const auto& resolution = transcodingResolution(
            streams.lowResolution, streams.highResolution, videoQuality, input.transcodingCodec);

        Result result = applyTranscodingIfPossible(resolution, input);

        if (!result.isValid() && videoQuality >= kHighQualityFallbackThreshold)
            result = chooseHighStreamIfPossible(streams.highCodec, streams.highResolution, input);

        if (!result.isValid())
            return lowQuality;

        return result;
    }

    return {};
}

static Result chooseFallbackQuality(const Streams& streams, const Params& input)
{
    if (streams.lowCodec != AV_CODEC_ID_NONE && streams.lowResolution.isValid())
        return {Player::LowVideoQuality, streams.lowResolution};

    const auto& resolution = transcodingResolution(
        streams.lowResolution,
        streams.highResolution,
        kFallbackLowQualityLines,
        input.transcodingCodec);

    const auto& result = applyTranscodingIfPossible(resolution, input);

    if (result.isValid())
        return result;

    if (streams.highCodec != AV_CODEC_ID_NONE && streams.highResolution.isValid())
        return chooseHighStreamIfPossible(streams.highCodec, streams.highResolution, input);

    return {};
}

} // namespace

Result chooseVideoQuality(const int videoQuality, const Params& input)
{
    if (!NX_ASSERT(input.camera) || !NX_ASSERT(input.currentDecoders))
        return {};

    const Streams streams = findCameraStreams(input.camera);

    Result result;

    // If high requested, high stream exists but has an unknown resolution, return high.
    if (videoQuality == Player::HighVideoQuality
        && streams.highCodec != AV_CODEC_ID_NONE && !streams.highResolution.isValid())
    {
        NX_DEBUG(kLogTag, "High stream requested but high stream resolution is unknown.");
        result = {Player::HighVideoQuality, QSize()};
    }
    // If low requested, low stream exists but has an unknown resolution, return low.
    else if (videoQuality == Player::LowVideoQuality
        && streams.lowCodec != AV_CODEC_ID_NONE && !streams.lowResolution.isValid())
    {
        NX_DEBUG(kLogTag, "Low stream requested but low stream resolution is unknown.");
        result = {Player::LowVideoQuality, QSize()};
    }
    else
    {
        result = choosePreferredQuality(streams, videoQuality, input);

        if (result.quality == Player::LowVideoQuality)
            result = chooseFallbackQuality(streams, input);
    }

    NX_DEBUG(kLogTag, nx::format("Requested %1 => Set %2").args(
        videoQualityToString(videoQuality), result));
    return result;
}

Player::TranscodingSupportStatus transcodingSupportStatus(
    const QnVirtualCameraResourcePtr& camera,
    qint64 positionMs,
    bool liveMode)
{
    const QnMediaServerResourcePtr& server = liveMode
        ? camera->getParentServer()
        : camera->systemContext()->cameraHistoryPool()->getMediaServerOnTime(camera, positionMs);

    if (!server)
        return Player::TranscodingNotSupported;

    if (server->getServerFlags().testFlag(vms::api::SF_SupportsTranscoding))
        return Player::TranscodingSupported;

    if (server->getVersion() < nx::utils::SoftwareVersion(3, 0))
        return Player::TranscodingNotSupportedForServersOlder30;

    return Player::TranscodingDisabled;
}

Result::Result(Player::VideoQuality quality, const QSize& frameSize):
    quality(quality),
    frameSize(frameSize)
{
}

bool Result::isValid() const
{
    return quality != Player::UnknownVideoQuality && frameSize.isValid();
}

bool Result::operator==(const Result& other) const
{
    return quality == other.quality && frameSize == other.frameSize;
}

QString Result::toString() const
{
    if (quality > Player::CustomVideoQuality)
        return nx::format("VideoQuality{%1}").arg((int) quality);

    auto result = QString::fromStdString(nx::reflect::toString(quality));
    if (frameSize != QSize())
        result += nx::format("[%1 x %2]").args(frameSize.width(), frameSize.height());

    return result;
}

} // namespace media_player_quality_chooser
} // namespace media
} // namespace nx
