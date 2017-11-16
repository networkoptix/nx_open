#include "legacy_transcoding_settings.h"

#include <core/resource/media_resource.h>

namespace nx {
namespace core {
namespace transcoding {

bool LegacyTranscodingSettings::isEmpty() const
{
    if (!qFuzzyIsNull(forcedAspectRatio))
        return false;

    const auto layout = resource->getVideoLayout();
    if (layout && layout->channelCount() > 1)
        return false;

    if (!zoomWindow.isEmpty())
        return false;

    if (itemDewarpingParams.enabled)
        return false;

    if (contrastParams.enabled)
        return false;

    if (rotation != 0)
        return false;

    if (timestampParams.enabled)
        return false;

    return true;
}

} // namespace transcoding
} // namespace core
} // namespace nx