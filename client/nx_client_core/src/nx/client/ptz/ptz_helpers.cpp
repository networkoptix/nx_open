#include "ptz_helpers.h"

#include <nx/utils/string.h>
#include <core/ptz/abstract_ptz_controller.h>

namespace nx {
namespace client {
namespace core {
namespace ptz {
namespace helpers {

QnPtzPresetList getSortedPresets(const QnAbstractPtzController* controller)
{
    if (!controller)
        return QnPtzPresetList();

    QnPtzPresetList presets;
    if (!controller->getPresets(&presets))
        return QnPtzPresetList();

    return sortedPresets(presets);
}

QnPtzPresetList getSortedPresets(const QnPtzControllerPtr& controller)
{
    return getSortedPresets(controller.data());
}

QnPtzPresetList sortedPresets(QnPtzPresetList presets)
{
    std::sort(presets.begin(), presets.end(),
        [](const QnPtzPreset &l, const QnPtzPreset &r)
        {
            return nx::utils::naturalStringLess(l.name, r.name);
        });

    return presets;
}

} // namespace helpers
} // namespace ptz
} // namespace core
} // namespace client
} // namespace nx

