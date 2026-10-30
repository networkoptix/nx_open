// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_stream_capability.h"

#include <algorithm>
#include <ranges>

#include <nx/fusion/model_functions.h>

namespace nx::vms::api {

float CameraStreamCapability::calcMaxFps() const noexcept
{
    return availableFps.empty() ? maxFps : *availableFps.rbegin();
}

QString CameraStreamCapability::toString() const
{
    return availableFps.empty()
        ? nx::format("Bitrate: %1-%2(%3) Kbps, FPS: %4, fps range: [%5, %6]").args(
            minBitrateKbps, maxBitrateKbps, defaultBitrateKbps, defaultFps, minFps, maxFps)
        : nx::format("Bitrate: %1-%2(%3) Kbps, FPS: %4, available fps: [%5]").args(
            minBitrateKbps, maxBitrateKbps, defaultBitrateKbps, defaultFps, availableFps);
}

void CameraStreamCapability::setRangeFps(const float maxFps, const float minFps/* = 1.0*/)
{
    this->maxFps = maxFps;
    this->minFps = minFps;
}

float CameraStreamCapability::strictFps(const float fps) const noexcept
{
    const auto findFloorFps = [this](float fps) -> std::optional<float>
    {
        if (availableFps.empty()) return std::nullopt;
        // Compared as float: fps is often a value of availableFps narrowed to float, and 3.33f
        // is less than 3.33.
        auto it =
            std::ranges::upper_bound(availableFps, fps, {}, [](double v) { return (float) v; });
        return (it != availableFps.begin()) ? *std::prev(it) : *it;
    };
    const auto tryClamp = [this](float fps)
    {
        return isRangeFpsValid() ? std::clamp(fps, minFps, maxFps) : fps;
    };
    const auto clampedFps = tryClamp(fps);
    return findFloorFps(clampedFps).value_or(clampedFps);
}

bool CameraStreamCapability::isRangeFpsValid() const noexcept
{
    return minFps < maxFps;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraStreamCapability, (json), CameraStreamCapability_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraMediaCapability, (json), CameraMediaCapability_Fields)

} // namespace nx::vms::api
