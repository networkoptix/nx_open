#pragma once

#include <core/ptz/ptz_fwd.h>

namespace nx {
namespace client {
namespace core {
namespace ptz {
namespace helpers {

QnPtzPresetList getSortedPresets(const QnAbstractPtzController* controller);
QnPtzPresetList getSortedPresets(const QnPtzControllerPtr& controller);

QnPtzPresetList sortedPresets(QnPtzPresetList presets);

} // namespace helpers
} // namespace ptz
} // namespace core
} // namespace client
} // namespace nx
