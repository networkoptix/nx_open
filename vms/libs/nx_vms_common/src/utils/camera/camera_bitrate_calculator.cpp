// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "camera_bitrate_calculator.h"

#include <QtCore/QMap>
#include <QtCore/QString>

#include <core/resource/camera_resource.h>
#include <nx/utils/math/math.h>
#include <nx/utils/log/log.h>
#include <nx/utils/log/log_main.h>

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
    const bool useDefaultResolution = resolution.isEmpty();
    if (useDefaultResolution)
        resolution = kDefaultResolution;

    const float resolutionFactor = kResolutionFactorMultiplier * pow(
        resolution.width() * resolution.height(),
        kResolutionFactorPower);

    const float frameRateFactor = kFpsFactorMultiplier * fps;

    const float bitrateMultiplier = kBitrateMultiplierByCodec.value(codec, 1.0f);

    float result = qualityFactor * resolutionFactor * frameRateFactor * bitrateMultiplier;
    NX_DEBUG(NX_SCOPE_TAG,
        "Bitrate factors bitrate=%1 Kbps: qualityFactor=%2, resolutionFactor=%3 (resolution=%4x%5), frameRateFactor=%6 (fps=%7), bitrateMultiplier=%8 (codec=%9)",
        result,
        qualityFactor,
        resolutionFactor,
        resolution.width(),
        resolution.height(),
        frameRateFactor,
        fps,
        bitrateMultiplier,
        codec);

    result = std::max(kMaxSuggestedBitrateKbps, result);
    NX_DEBUG(NX_SCOPE_TAG,
        "Suggest bitrate for quality %1, %2 resolution %3x%4, fps %5, codec %6: %7 Kbps",
        quality,
        useDefaultResolution ? "default" : "",
        resolution.width(),
        resolution.height(),
        fps,
        codec,
        result);

    return std::round(result);
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
        const auto bitrate = qBound(
            streamCapability.minBitrateKbps,
            streamCapability.defaultBitrateKbps * coefficient * ((float)fps / streamCapability.defaultFps),
            streamCapability.maxBitrateKbps
        );

        NX_DEBUG(NX_SCOPE_TAG,
            "Suggest bitrate for default bitrate %1 Kbps, min bitrate %2 Kbps, max bitrate %3 Kbps, quality %4, fps %5, default fps %6: %7 Kbps",
            streamCapability.defaultBitrateKbps,
            streamCapability.minBitrateKbps,
            streamCapability.maxBitrateKbps,
            quality,
            fps,
            streamCapability.defaultFps,
            bitrate);

        return bitrate;
    }

    auto result = suggestBitrateForQualityKbps(quality, resolution, fps, codec);

    if (useBitratePerGop && fps > 0)
    {
        result = result * (kDefaultBitratePerGop / (float) fps);
        NX_DEBUG(NX_SCOPE_TAG, "Adjusting bitrate per GOP: fps %1, result %2 Kbps", fps, result);
    }

    if (streamCapability.maxBitrateKbps > 0)
    {
        result = qBound(
            streamCapability.minBitrateKbps,
            result,
            streamCapability.maxBitrateKbps);

        NX_DEBUG(NX_SCOPE_TAG,
            "Clamped bitrate: [%1, %2], final %3 Kbps",
            streamCapability.minBitrateKbps,
            streamCapability.maxBitrateKbps,
            result);
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
