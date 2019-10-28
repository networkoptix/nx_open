#include "led_manager.h"

namespace nx::vms::server::nvr::hanwha {

std::vector<ILedManager::LedDescriptor> LedManager::ledDescriptors() const
{
    // TODO: #dmishin implement.
    return {};
}

std::vector<ILedManager::LedState> LedManager::ledStates() const
{
    // TODO: #dmishin implement.
    return {};
}

bool LedManager::enable(const QString& ledId)
{
    // TODO: #dmishin implement.
    return false;
}

bool LedManager::disable(const QString& ledId)
{
    // TODO: #dmishin implement.
    return false;
}

} // namespace nx::vms::server::nvr::hanwha
