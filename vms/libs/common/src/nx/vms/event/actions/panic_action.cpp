#include "panic_action.h"

#include <core/resource/resource.h>

namespace nx {
namespace vms {
namespace event {

PanicAction::PanicAction(const EventParameters& runtimeParams):
    base_type(ActionType::panicRecordingAction, runtimeParams)
{
}

} // namespace event
} // namespace vms
} // namespace nx
