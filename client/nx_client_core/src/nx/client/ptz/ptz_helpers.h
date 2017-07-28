#pragma once

#include <core/ptz/ptz_fwd.h>
#include <core/resource/resource_fwd.h>

namespace nx {
namespace client {
namespace core {
namespace ptz {
namespace helpers {

bool getSortedPresets(const QnAbstractPtzController* controller, QnPtzPresetList& presets);
bool getSortedPresets(const QnPtzControllerPtr& controller, QnPtzPresetList& presets);

QnPtzPresetList sortedPresets(const QnResourcePtr& resource, QnPtzPresetList presets);

} // namespace helpers
} // namespace ptz
} // namespace core
} // namespace client
} // namespace nx
