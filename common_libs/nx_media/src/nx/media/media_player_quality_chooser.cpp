#include "media_player_quality_chooser.h"

#include <QtCore/QSize>

#include <common/common_module.h>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/log.h>

#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>

#include "video_decoder_registry.h"
#include "media_player.h" //< For enum VideoQuality.

namespace nx {
namespace media {
namespace media_player_quality_chooser {

namespace {

static const int kFallbackLowQualityLines = 360;
static const int kHighQualityFallbackThreshold = 560;
static const QSize kMaxTranscodingResolution(1920, 1080);
static const QSize kDefaultAspect(16, 9);
static const int kWidthRoundingFactor = 16;
static const int kHeightRoundingFactor = 4;

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

/**
 * @return Transcoding resolution, or invalid (default) QSize if transcoding is not possible.
 */
static Result applyTranscodingIfPossible(
    AVCodecID transcodingCodec,
    bool liveMode,
    qint64 positionMs,
    const QnVirtualCameraResourcePtr& camera,
    bool allowOverlay,
    const QSize& desiredResolution,
    const std::vector<AbstractVideoDecoder*>& currentDecoders)
{
    if (transcodingSupportStatus(camera, positionMs, liveMode, TranscodingRequestType::simple)
        != Player::TranscodingSupported)
    {
        NX_LOG(lit("[media_player] Transcoding is not supported for the camera."),
            cl_logDEBUG1);
        return Result();
    }

    QSize resolution = limitResolution(desiredResolution, kMaxTranscodingResolution);

    if (!VideoDecoderRegistry::instance()->hasCompatibleDecoder(
        transcodingCodec, resolution, allowOverlay, currentDecoders))
    {
        NX_LOG(lit("[media_player] Transcoding to %1 x %2 not supported.")
            .arg(resolution.width()).arg(resolution.height()), cl_logDEBUG1);
        return Result();
    }

    NX_LOG(lit("[media_player] Set transcoding to %1 x %2.")
        .arg(resolution.width()).arg(resolution.height()), cl_logDEBUG1);
    return Result(Player::CustomVideoQuality, resolution);
}

static Result chooseHighStreamIfPossible(
    AVCodecID transcodingCodec,
    bool liveMode,
    qint64 positionMs,
    const QnVirtualCameraResourcePtr& camera,
    bool allowOverlay,
    AVCodecID highCodec,
    const QSize& highResolution,
    const std::vector<AbstractVideoDecoder*>& currentDecoders)
{
    if (highCodec == AV_CODEC_ID_NONE || !highResolution.isValid()) //< High stream doesn't exist.
    {
        NX_LOG(lit("[media_player] High stream requested but missing."), cl_logDEBUG1);
        return Result();
    }

    if (camera->getVideoLayout()->channelCount() > 1) //< Panoramic camera.
    {
        const QSize& maxResolution =
            VideoDecoderRegistry::instance()->maxResolution(transcodingCodec);

        const QSize& resolution = limitResolution(highResolution, maxResolution);

        NX_LOG(
            lit("[media_player] Panoramic camera: "
                "High stream requested => Attempt transcoding to %2 x %3:")
                .arg(resolution.width()).arg(resolution.height()),
            cl_logDEBUG1);

        return applyTranscodingIfPossible(
            transcodingCodec,
            liveMode,
            positionMs,
            camera,
            allowOverlay,
            resolution,
            currentDecoders);
    }
    else if (VideoDecoderRegistry::instance()->hasCompatibleDecoder(
        highCodec, highResolution, allowOverlay, currentDecoders))
    {
        return Result(Player::HighVideoQuality, highResolution);
    }
    else
    {
        NX_LOG(lit("[media_player] High stream requested but compatible decoder is missing:"),
            cl_logDEBUG1);

        return applyTranscodingIfPossible(
            transcodingCodec,
            liveMode,
            positionMs,
            camera,
            allowOverlay,
            highResolution,
            currentDecoders);
    }

    return Result();
}

static Result chooseFallbackQuality(
    AVCodecID transcodingCodec,
    bool liveMode,
    qint64 positionMs,
    const QnVirtualCameraResourcePtr& camera,
    bool allowOverlay,
    AVCodecID lowCodec,
    const QSize& lowResolution,
    AVCodecID highCodec,
    const QSize& highResolution,
    const std::vector<AbstractVideoDecoder*>& currentDecoders)
{
    if (lowCodec != AV_CODEC_ID_NONE && lowResolution.isValid())
        return Result(Player::LowVideoQuality, lowResolution);

    const auto& resolution = transcodingResolution(
        lowResolution, highResolution, kFallbackLowQualityLines, transcodingCodec);

    const auto& result = applyTranscodingIfPossible(
        transcodingCodec, liveMode, positionMs, camera, allowOverlay, resolution, currentDecoders);

    if (result.isValid())
        return result;

    if (highCodec != AV_CODEC_ID_NONE && highResolution.isValid())
    {
        return chooseHighStreamIfPossible(
            transcodingCodec,
            liveMode,
            positionMs,
            camera,
            allowOverlay,
            highCodec,
            highResolution,
            currentDecoders);
    }

    return Result();
}

static QString qualityString(const Result& quality)
{
    return quality.toString();
}

static QString qualityString(int quality)
{
    if (quality >= Player::CustomVideoQuality)
        return qualityString(Result(Player::CustomVideoQuality, QSize(-1, quality)));

    return qualityString(Result(static_cast<Player::VideoQuality>(quality), QSize()));
}

} // namespace

Result chooseVideoQuality(
    AVCodecID transcodingCodec,
    int videoQuality,
    bool liveMode,
    qint64 positionMs,
    const QnVirtualCameraResourcePtr& camera,
    bool allowOverlay,
    const std::vector<AbstractVideoDecoder*>& currentDecoders)
{
    // Obtain Low and High stream codec and resolution.
    QSize highResolution;
    AVCodecID highCodec = AV_CODEC_ID_NONE;
    QSize lowResolution;
    AVCodecID lowCodec = AV_CODEC_ID_NONE;
    findCameraStreams(camera, &highResolution, &highCodec, &lowResolution, &lowCodec);

    const auto lowQuality = Result(Player::LowVideoQuality, lowResolution);

    const bool highStreamRequested = videoQuality == Player::HighVideoQuality
        || (highCodec != AV_CODEC_ID_NONE && highResolution.isValid()
            && videoQuality == highResolution.height());

    const bool lowStreamRequested = videoQuality == Player::LowVideoQuality
        || (lowCodec != AV_CODEC_ID_NONE && lowResolution.isValid()
            && videoQuality == lowResolution.height());

    Result result;

    auto logResult =
        [videoQuality, &result]()
        {
            NX_LOG(
                lit("[media_player] Requested %1 => Set %2")
                    .arg(qualityString(videoQuality), qualityString(result)),
                cl_logDEBUG1);
        };

    if (videoQuality == Player::LowIframesOnlyVideoQuality)
    {
        if (lowQuality.isValid())
        {
            result = Result(Player::LowIframesOnlyVideoQuality, lowResolution);
        }
        else
        {
            // Player::LowIframesOnlyVideoQuality is used when extreme low CPU and network
            // usage is required and other quality options are not acceptable. So we don't try
            // another options in this case.
            result = Result();
        }
    }
    else if (highStreamRequested)
    {
        result = chooseHighStreamIfPossible(
            transcodingCodec,
            liveMode,
            positionMs,
            camera,
            allowOverlay,
            highCodec,
            highResolution,
            currentDecoders);

        if (!result.isValid())
            result = lowQuality;
    }
    else if (lowStreamRequested)
    {
        result = lowQuality;
    }
    else if (videoQuality > Player::CustomVideoQuality) //< Custom lines count requested.
    {
        const auto& resolution = transcodingResolution(
            lowResolution, highResolution, videoQuality, transcodingCodec);

        result = applyTranscodingIfPossible(
            transcodingCodec,
            liveMode,
            positionMs,
            camera,
            allowOverlay,
            resolution,
            currentDecoders);

        if (!result.isValid() && videoQuality >= kHighQualityFallbackThreshold)
        {
            result = chooseHighStreamIfPossible(
                transcodingCodec,
                liveMode,
                positionMs,
                camera,
                allowOverlay,
                highCodec,
                highResolution,
                currentDecoders);
        }

        if (!result.isValid())
            result = lowQuality;
    }

    if (result.quality == Player::LowVideoQuality)
    {
        result = chooseFallbackQuality(
            transcodingCodec,
            liveMode,
            positionMs,
            camera,
            allowOverlay,
            lowCodec,
            lowResolution,
            highCodec,
            highResolution,
            currentDecoders);
    }

    logResult();
    return result;
}

Player::TranscodingSupportStatus transcodingSupportStatus(
    const QnVirtualCameraResourcePtr& camera,
    qint64 positionMs,
    bool liveMode,
    TranscodingRequestType requestType)
{
    if (!VideoDecoderRegistry::instance()->isTranscodingEnabled())
        return Player::TranscodingDisabled;

    const QnMediaServerResourcePtr& server = liveMode
        ? camera->getParentServer()
        : camera->commonModule()->cameraHistoryPool()->getMediaServerOnTime(camera, positionMs);

    if (!server)
        return Player::TranscodingNotSupported;

    if (server->getServerFlags().testFlag(Qn::SF_SupportsTranscoding))
        return Player::TranscodingSupported;

    if (requestType == TranscodingRequestType::detailed)
    {
        if (server->getVersion() < QnSoftwareVersion(3, 0))
            return Player::TranscodingNotSupportedForServersOlder30;

        const auto& info = server->getSystemInfo();
        if (info.arch == lit("arm") || info.arch == lit("aarch64"))
            return Player::TranscodingNotSupportedForArmServers;
    }

    return Player::TranscodingNotSupported;
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
    QString result = QnLexical::serialized(quality);

    if (frameSize.height() >= 0)
    {
        if (frameSize.width() >= 0)
            result += lit(" [%1 x %2]").arg(frameSize.width()).arg(frameSize.height());
        else
            result += lit(" [h: %1]").arg(frameSize.height());
    }

    return result;
}

} // namespace media_player_quality_chooser
} // namespace media
} // namespace nx
