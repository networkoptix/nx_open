#pragma once

#include <core/ptz/ptz_fwd.h>

namespace nx {
namespace client {
namespace ptz {
namespace helpers {

QnPtzPresetList getSortedPresets(const QnAbstractPtzController* controller);
QnPtzPresetList getSortedPresets(const QnPtzControllerPtr& controller);

QnPtzPresetList sortedPresets(QnPtzPresetList presets);

} // namespace helpers
} // namespace ptz
} // namespace client
} // namespace nx
