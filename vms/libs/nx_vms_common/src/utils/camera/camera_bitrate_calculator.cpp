// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bitrate_calculator.h"

#include <QtCore/QMap>
#include <QtCore/QString>

#include <core/resource/camera_resource.h>
#include <nx/utils/math/math.h>

namespace {

static constexpr float kKbpsInMbps = 1000.0f;
static constexpr float kLowEndBitrateKbps = 0.1f;
static constexpr float kHighEndBitrateKbps = 1.0f;
static constexpr float kResolutionFactorPower = 0.7f;
static constexpr float kResolutionFactorMultiplier = 0.009f;
static constexpr float kFpsFactorMultiplier = 1.0f;
static constexpr float kMaxSuggestedBitrateKbps = 192.0f;
static constexpr float kDefaultBitratePerGop = 30.0f;

static constexpr QSize kDefaultResolution{1920, 1080};

static const QMap<QString, float> kBitrateMultiplierByCodec =
    { { "MJPEG", 2.0f }, { "H264", 1.0f }, { "H265", 0.8 } };

constexpr float bitrateCoefficient(Qn::StreamQuality quality)
{
    switch (quality)
    {
        case Qn::StreamQuality::lowest:
            return 0.33f;
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

namespace nx::vms::common {

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

    // Use a predefined value for cameras which did not fill the resolution yet.
    if (resolution.isEmpty())
        resolution = kDefaultResolution;

    const float resolutionFactor = kResolutionFactorMultiplier * pow(
        resolution.width() * resolution.height(),
        kResolutionFactorPower);

    const float frameRateFactor = kFpsFactorMultiplier * fps;

    const float bitrateMultiplyer = kBitrateMultiplierByCodec.value(codec, 1.0f);

    const float result = qualityFactor * resolutionFactor * frameRateFactor * bitrateMultiplyer;

    return std::round(std::max(kMaxSuggestedBitrateKbps, result));
}

float CameraBitrateCalculator::suggestBitrateForQualityKbps(
    Qn::StreamQuality quality,
    QSize resolution,
    int fps,
    const QString& codec,
    nx::vms::api::CameraStreamCapability streamCapability,
    bool useBitratePerGop)
{
    if (streamCapability.defaultBitrateKbps > 0 && streamCapability.defaultFps > 0)
    {
        const auto coefficient = bitrateCoefficient(quality);
        const auto bitrate = streamCapability.defaultBitrateKbps * coefficient;
        return qBound(
            streamCapability.minBitrateKbps,
            bitrate * ((float)fps / streamCapability.defaultFps),
            streamCapability.maxBitrateKbps);
    }

    auto result = suggestBitrateForQualityKbps(quality, resolution, fps, codec);

    if (useBitratePerGop && fps > 0)
        result = result * (kDefaultBitratePerGop / (float)fps);

    if (streamCapability.maxBitrateKbps > 0)
    {
        result = qBound(
            streamCapability.minBitrateKbps,
            result,
            streamCapability.maxBitrateKbps);
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

} // namespace nx::vms::common
