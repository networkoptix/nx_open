#include "media_player_quality_chooser.h"

#include <QtCore/QSize>

#include <nx/utils/log/log.h>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>

#include "video_decoder_registry.h"
#include "media_player.h" //< For enum VideoQuality.

namespace nx {
namespace media {

namespace {

static const QSize kMaxTranscodingResolution(1920, 1080);
static const QSize kDefaultAspect(16, 9);

/**
 * If a particular stream is missing, its out parameters are not assigned a value.
 */
void findCameraStreams(
    const QnVirtualCameraResourcePtr& camera,
    QSize* outHighResolution,
    AVCodecID* outHighCodec,
    QSize* outLowResolution,
    AVCodecID* outLowCodec)
{
    for (const auto& stream: camera->mediaStreams().streams)
    {
        if (stream.encoderIndex == CameraMediaStreamInfo::PRIMARY_STREAM_INDEX) //< High
        {
            *outHighCodec = (AVCodecID) stream.codec;
            *outHighResolution = stream.getResolution();
        }
        else if (stream.encoderIndex == CameraMediaStreamInfo::SECONDARY_STREAM_INDEX) //< Low
        {
            *outLowCodec = (AVCodecID) stream.codec;
            *outLowResolution = stream.getResolution();
        }
    }
}

static QSize limitResolution(const QSize& desiredResolution, const QSize& limit)
{
    if (desiredResolution.isEmpty() || limit.isEmpty())
        return desiredResolution;

    QSize result = desiredResolution;

    if (result.width() > limit.width())
    {
        result.setHeight((result.height() * limit.width()) / result.width());
        result.setWidth(limit.width());
    }

    if (result.height() > limit.height())
    {
        result.setWidth((result.width() * limit.height()) / result.height());
        result.setHeight(limit.height());
    }

    NX_ASSERT(result.width() <= limit.width());
    NX_ASSERT(result.height() <= limit.height());

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
        NX_LOG(lit("[media_player] Custom resolution of %1p requested; "
            "desired resolution is %2 x %3:")
            .arg(videoQuality).arg(desiredResolution.width()).arg(desiredResolution.height()),
            cl_logDEBUG1);
    }
    else
    {
        NX_LOG(lit("[media_player] Custom resolution of %1p requested; "
            "desired resolution is %2 x %3, limited to %4 x %5:")
            .arg(videoQuality).arg(desiredResolution.width()).arg(desiredResolution.height())
            .arg(result.width()).arg(result.height()),
            cl_logDEBUG1);
    }

    return result;
}

static bool isTranscodingSupported(
    bool liveMode, qint64 positionMs, const QnVirtualCameraResourcePtr& camera)
{
    if (!VideoDecoderRegistry::instance()->isTranscodingEnabled())
        return false;

    QnMediaServerResourcePtr server = liveMode
        ? camera->getParentServer()
        : qnCameraHistoryPool->getMediaServerOnTime(camera, positionMs);

    if (!server)
        return false;

    return server->getServerFlags().testFlag(Qn::SF_SupportsTranscoding);
}

/**
 * @return Transcoding resolution, or invalid (default) QSize if transcoding is not possible.
 */
static QSize applyTranscodingIfPossible(
    AVCodecID transcodingCodec,
    bool liveMode,
    qint64 positionMs,
    const QnVirtualCameraResourcePtr& camera,
    const QSize& desiredResolution)
{
    if (!isTranscodingSupported(liveMode, positionMs, camera))
    {
        NX_LOG(lit("[media_player] Transcoding not supported for the camera => Set low stream"),
            cl_logDEBUG1);
        return QSize();
    }

    QSize resolution = limitResolution(desiredResolution, kMaxTranscodingResolution);

    if (!VideoDecoderRegistry::instance()->hasCompatibleDecoder(
        transcodingCodec, resolution))
    {
        NX_LOG(lit("[media_player] Transcoding to %1 x %2 not supported => Set low stream")
            .arg(resolution.width()).arg(resolution.height()), cl_logDEBUG1);
        return QSize();
    }

    NX_LOG(lit("[media_player] Set transcoding to %1 x %2")
        .arg(resolution.width()).arg(resolution.height()), cl_logDEBUG1);
    return resolution;
}

} // namespace

const QSize media_player_quality_chooser::kQualityLow{-1, Player::LowVideoQuality};
const QSize media_player_quality_chooser::kQualityHigh{-1, Player::HighVideoQuality};
const QSize media_player_quality_chooser::kQualityLowIframesOnly{-1,
    Player::LowIframesOnlyVideoQuality};

QSize media_player_quality_chooser::chooseHighStreamIfPossible(
    AVCodecID transcodingCodec,
    bool liveMode,
    qint64 positionMs,
    const QnVirtualCameraResourcePtr& camera,
    AVCodecID highCodec,
    const QSize& highResolution,
    const QnConstResourceVideoLayoutPtr& videoLayout)
{
    if (highCodec != AV_CODEC_ID_NONE && !highResolution.isEmpty()) //< High stream exists.
    {
        if (videoLayout->channelCount() > 1) //< Panoramic camera.
        {
            const QSize& maxResolution =
                VideoDecoderRegistry::instance()->maxResolution(transcodingCodec);

            const QSize& resolution = limitResolution(highResolution, maxResolution);

            NX_LOG(lit("[media_player] Panoramic camera: "
                "High stream requested => Attempt transcoding to %2 x %3:")
                .arg(resolution.width()).arg(resolution.height()),
                cl_logDEBUG1);

            const QSize& quality = applyTranscodingIfPossible(
                transcodingCodec, liveMode, positionMs, camera, resolution);
            if (quality.isValid())
                return quality;
        }
        else if (VideoDecoderRegistry::instance()->hasCompatibleDecoder(
            highCodec, highResolution))
        {
            NX_LOG(lit("[media_player] High stream requested => Set high stream"),
                cl_logDEBUG1);
            return kQualityHigh;
        }
        else
        {
            NX_LOG(lit("[media_player] High stream requested but compatible decoder missing:"),
                cl_logDEBUG1);
            const QSize& quality = applyTranscodingIfPossible(
                transcodingCodec, liveMode, positionMs, camera, highResolution);
            if (quality.isValid())
                return quality;
        }
    }
    else
    {
        NX_LOG(lit("[media_player] High stream requested but missing => Set low stream"),
            cl_logDEBUG1);
    }

    return kQualityLow;
}

media_player_quality_chooser::Result media_player_quality_chooser::chooseVideoQuality(
    AVCodecID transcodingCodec,
    int videoQuality,
    bool liveMode,
    qint64 positionMs,
    const QnVirtualCameraResourcePtr& camera)
{
    // Obtain Low and High stream codec and resolution.
    QSize highResolution;
    AVCodecID highCodec = AV_CODEC_ID_NONE;
    QSize lowResolution;
    AVCodecID lowCodec = AV_CODEC_ID_NONE;
    findCameraStreams(camera, &highResolution, &highCodec, &lowResolution, &lowCodec);

    auto makeResult =
        [&lowResolution, &highResolution](const QSize& quality)
        {
            switch (quality.height())
            {
                case Player::LowVideoQuality:
                    return Result{Player::LowVideoQuality, lowResolution};
                case Player::LowIframesOnlyVideoQuality:
                    return Result{Player::LowIframesOnlyVideoQuality, lowResolution};
                case Player::HighVideoQuality:
                    return Result{Player::HighVideoQuality, highResolution};
                default:
                    return Result{Player::CustomVideoQuality, quality};
            }
        };

    QN_UNUSED(lowCodec);

    if (videoQuality == Player::LowIframesOnlyVideoQuality)
    {
        NX_LOG(
            lit("[media_player] Low stream I-frames only requested => "
                "Set low stream I-frames only"),
            cl_logDEBUG1);
        return makeResult(kQualityLowIframesOnly);
    }

    const bool highStreamRequested = videoQuality == Player::HighVideoQuality
        || (!highResolution.isEmpty() && videoQuality == highResolution.height());

    const bool lowStreamRequested = videoQuality == Player::LowVideoQuality
        || (!lowResolution.isEmpty() && videoQuality == lowResolution.height());

    const auto& videoLayout = camera->getVideoLayout();

    if (highStreamRequested)
    {
        const QSize& quality = chooseHighStreamIfPossible(
            transcodingCodec, liveMode, positionMs, camera,
            highCodec, highResolution, videoLayout);
        return makeResult(quality);
    }
    else if (lowStreamRequested)
    {
        NX_LOG(lit("[media_player] Low stream requested => Set low stream"), cl_logDEBUG1);
    }
    else if (videoQuality >= Player::CustomVideoQuality) //< Custom lines count requested.
    {
        const QSize& resolution = transcodingResolution(
            lowResolution, highResolution, videoQuality, transcodingCodec);
        const QSize& quality = applyTranscodingIfPossible(
            transcodingCodec, liveMode, positionMs, camera, resolution);
        if (quality.isValid())
            return makeResult(quality);
    }
    else
    {
        NX_LOG(lit("[media_player] Invalid video quality %1 => Set low stream")
            .arg(videoQuality), cl_logDEBUG1);
    }

    return makeResult(kQualityLow);
}

} // namespace media
} // namespace nx
