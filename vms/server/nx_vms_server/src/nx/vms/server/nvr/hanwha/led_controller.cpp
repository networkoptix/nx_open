#include "led_controller.h"

namespace nx::vms::server::nvr::hanwha {

std::vector<ILedController::LedDescriptor> LedController::ledDescriptors() const
{
    // TODO: #dmishin implement.
    return {};
}

std::map<ILedController::LedId, ILedController::LedState> LedController::ledStates() const
{
    // TODO: #dmishin implement.
    return {};
}

bool LedController::enable(const QString& ledId)
{
    // TODO: #dmishin implement.
    return false;
}

bool LedController::disable(const QString& ledId)
{
    // TODO: #dmishin implement.
    return false;
}

} // namespace nx::vms::server::nvr::hanwha
