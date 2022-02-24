// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "media_stream_capability.h"
#include <nx/fusion/model_functions.h>

namespace nx {
namespace media {

CameraStreamCapability::CameraStreamCapability(float minBitrate, float maxBitrate, int fps)
:
    minBitrateKbps(minBitrate), maxBitrateKbps(maxBitrate), defaultBitrateKbps(maxBitrate),
    defaultFps(fps), maxFps(fps)
{
}

bool CameraStreamCapability::isNull() const
{
    return maxFps == 0;
}

QString CameraStreamCapability::toString() const
{
    return nx::format("Bitrate: %1-%2(%3) Kbps, FPS: %4(%5)").args(
        minBitrateKbps, maxBitrateKbps, defaultBitrateKbps, maxFps, defaultFps);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraStreamCapability, (json), CameraStreamCapability_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraMediaCapability, (json), CameraMediaCapability_Fields)

} // namespace media
} // namespace nx
