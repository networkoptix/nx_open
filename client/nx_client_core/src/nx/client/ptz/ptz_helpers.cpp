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

    const auto getPtzPresetHotkeyNumber =
        [presetIdHotkeyHash](const QString& id) -> int
        {
            /**
             * Since we have only digit hotkeys (0-9) we suppose that invalid preset number is
             * large enought to make sorting easer: each preset with hotkey is always less than
             * preset without it.
             */
            static const int kNoPresetNumberValue = 10;
            const auto it = presetIdHotkeyHash.find(id);
            return it == presetIdHotkeyHash.end() ? kNoPresetNumberValue : it.value();
        };

    std::sort(presets.begin(), presets.end(),
        [getPtzPresetHotkeyNumber](const QnPtzPreset& left, const QnPtzPreset& right)
        {
            const int leftPresetNumber = getPtzPresetHotkeyNumber(left.id);
            const int rightPresetNumber = getPtzPresetHotkeyNumber(right.id);
            return leftPresetNumber == rightPresetNumber
                ? nx::utils::naturalStringLess(left.name, right.name)
                : leftPresetNumber < rightPresetNumber;
        });

    return presets;
}

} // namespace helpers
} // namespace ptz
} // namespace core
} // namespace client
} // namespace nx

