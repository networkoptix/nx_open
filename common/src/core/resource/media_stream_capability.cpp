#include "media_stream_capability.h"
#include <nx/fusion/model_functions.h>

namespace nx {
namespace media {

QString CameraStreamCapability::toString() const
{
    return lm("Bitrate: %1-%2(%3) Kbps, FPS: %4(%5)").args(
        minBitrateKbps, maxBitrateKbps, defaultBitrateKbps, maxFps, defaultFps);
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraStreamCapability, (json), CameraStreamCapability_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(CameraMediaCapability, (json), CameraMediaCapability_Fields)

} // namespace media
} // namespace nx
