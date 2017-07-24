#include "ptz_helpers.h"

#include <nx/utils/string.h>
#include <core/ptz/abstract_ptz_controller.h>
#include <nx/client/ptz/ptz_hotkey_resource_property_adaptor.h>

namespace nx {
namespace client {
namespace core {
namespace ptz {
namespace helpers {

bool getSortedPresets(const QnAbstractPtzController* controller, QnPtzPresetList& presets)
{
    if (!controller)
        return false;

    if (!controller->getPresets(&presets))
        return false;

    presets = sortedPresets(controller->resource(), presets);
    return true;
}

bool getSortedPresets(const QnPtzControllerPtr& controller, QnPtzPresetList& presets)
{
    return getSortedPresets(controller.data(), presets);
}

QnPtzPresetList sortedPresets(const QnResourcePtr& resource, QnPtzPresetList presets)
{
    PtzHotkeysResourcePropertyAdaptor adaptor;
    adaptor.setResource(resource);

    const auto presetIdHotkeyHash =
        [&adaptor]()
        {
            QHash<QString, int> result;
            const auto hotkeyPresetIdHash = adaptor.value();
            for (auto hotkey: hotkeyPresetIdHash.keys())
                result.insert(hotkeyPresetIdHash.value(hotkey), hotkey);
            return result;
        }();

    const auto getPtzPresetName =
        [presetIdHotkeyHash](const QnPtzPreset& preset) -> QString
        {
            const auto it = presetIdHotkeyHash. find(preset.id);
            if (it == presetIdHotkeyHash.end())
                return preset.name;

            return QString::number(it.value()) + preset.name;
        };

    std::sort(presets.begin(), presets.end(),
        [getPtzPresetName](const QnPtzPreset& left, const QnPtzPreset& right)
        {
            return nx::utils::naturalStringLess(
                getPtzPresetName(left),
                getPtzPresetName(right));
        });

    return presets;
}

} // namespace helpers
} // namespace ptz
} // namespace core
} // namespace client
} // namespace nx

