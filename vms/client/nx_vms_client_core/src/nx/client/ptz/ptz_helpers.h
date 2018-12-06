#pragma once

#include <core/ptz/ptz_fwd.h>
#include <core/resource/resource_fwd.h>

namespace nx::vms::client::core {
namespace ptz {
namespace helpers {

bool getSortedPresets(const QnAbstractPtzController* controller, QnPtzPresetList& presets);
bool getSortedPresets(const QnPtzControllerPtr& controller, QnPtzPresetList& presets);

QnPtzPresetList sortedPresets(const QnResourcePtr& resource, QnPtzPresetList presets);

} // namespace helpers
} // namespace ptz
} // namespace nx::vms::client::core
