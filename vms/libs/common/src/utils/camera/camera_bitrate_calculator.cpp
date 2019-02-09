#include "camera_bitrate_calculator.h"

#include <core/resource/camera_resource.h>

#include <QString>
#include <QMap>

namespace {

static constexpr float kKbpsInMbps = 1000.0f;
static constexpr float kLowEndBitrateKbps = 0.1f;
static constexpr float kHighEndBitrateKbps = 1.0f;
static constexpr float kResolutionFactorPower = 0.7f;
static constexpr float kResolutionFactorMultiplier = 0.009f;
static constexpr float kFpsFactorMultiplier = 1.0f;
static constexpr float kMaxSuggestedBitrateKbps = 192.0f;
static constexpr float kDefaultBitratePerGop = 30.0f;

static const QMap<QString, float> kBitrateMultiplyerByCodec =
    { { "MJPEG", 2.0f }, { "H264", 1.0f }, { "H265", 0.8 } };

constexpr float bitrateCoefficient(Qn::StreamQuality quality)
{
    switch (quality)
    {
        case Qn::StreamQuality::lowest:
            return 0.66f;
        case Qn::StreamQuality::low:
            return 0.8f;
        case Qn::StreamQuality::normal:
            return 1.0f;
        case Qn::StreamQuality::high:
            return 2.0f;
        case Qn::StreamQuality::highest:
            return 2.5f;
        default:
            return 1.0f;
    }
};

} // namespace

namespace nx {
namespace core {

float CameraBitrateCalculator::suggestBitrateForQualityKbps(
    Qn::StreamQuality quality,
    QSize resolution,
    int fps,
    const QString& codec)
{
    const float qualityLevel =
        static_cast<int>(quality) - static_cast<int>(Qn::StreamQuality::lowest);
    static constexpr int qualitySpread =
        static_cast<int>(Qn::StreamQuality::highest) - static_cast<int>(Qn::StreamQuality::lowest);
    const float qualityFactor = kLowEndBitrateKbps + (kHighEndBitrateKbps - kLowEndBitrateKbps)
        * qualityLevel / qualitySpread;

    const float resolutionFactor = kResolutionFactorMultiplier * pow(
        resolution.width() * resolution.height(),
        kResolutionFactorPower);

    const float frameRateFactor = kFpsFactorMultiplier * fps;

    const float bitrateMultiplyer = kBitrateMultiplyerByCodec.value(codec, 1.0f);

    const float result = qualityFactor * resolutionFactor * frameRateFactor * bitrateMultiplyer;

    return std::max(kMaxSuggestedBitrateKbps, result);
}

float CameraBitrateCalculator::suggestBitrateForQualityKbps(
    Qn::StreamQuality quality,
    QSize resolution,
    int fps,
    const QString& codec,
    media::CameraStreamCapability streamCapability,
    bool useBitratePerGop)
{
    if (streamCapability.defaultBitrateKbps > 0 && streamCapability.defaultFps > 0)
    {
        const auto coefficient = bitrateCoefficient(quality);
        const auto bitrate = streamCapability.defaultBitrateKbps * coefficient;
        return qBound(
            (float)streamCapability.minBitrateKbps,
            bitrate * ((float)fps / streamCapability.defaultFps),
            (float)streamCapability.maxBitrateKbps);
    }

    auto result = suggestBitrateForQualityKbps(quality, resolution, fps, codec);

    if (useBitratePerGop && fps > 0)
        result = result * (kDefaultBitratePerGop / (float)fps);

    if (streamCapability.maxBitrateKbps > 0)
    {
        result = qBound(
            (float)streamCapability.minBitrateKbps,
            result,
            (float)streamCapability.maxBitrateKbps);
    }

    return result;
}

float CameraBitrateCalculator::roundKbpsToMbps(float kbps, int decimals)
{
    const auto roundingStep = std::pow(0.1, decimals);
    return qRound(kbps / kKbpsInMbps, roundingStep);
}

float CameraBitrateCalculator::roundMbpsToKbps(float mbps, int decimals)
{
    const auto roundingStep = std::pow(0.1, decimals);
    return qRound(mbps * kKbpsInMbps, roundingStep);
}

float CameraBitrateCalculator::getBitrateForQualityMbps(
    const QnVirtualCameraResourcePtr& camera,
    Qn::StreamQuality quality,
    int fps,
    const QString& codec)
{
    const auto resolution = camera->streamInfo().getResolution();
    const auto bitrateKbps = camera->suggestBitrateForQualityKbps(
        quality,
        resolution,
        fps,
        codec,
        Qn::CR_LiveVideo);

    return roundKbpsToMbps(bitrateKbps, kBitrateKbpsPrecisionDecimals);
}

} // namespace core
} // namespace nx