#include "led_manager.h"

#include <nx/utils/log/log.h>

#include <nx/vms/server/nvr/hanwha/led/i_led_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

LedManager::LedManager(std::unique_ptr<ILedPlatformAbstraction> platformAbstraction):
    m_platformAbstraction(std::move(platformAbstraction))
{
    NX_DEBUG(this, "Creating the LED manager");
}

LedManager::~LedManager()
{
    NX_DEBUG(this, "Destroying the LED manager");
}

std::vector<LedDescription> LedManager::ledDescriptions() const
{
    NX_DEBUG(this, "LED descriptions are requested");
    return m_platformAbstraction->ledDescriptions();
}

bool LedManager::setLedState(const QString& ledId, LedState state)
{
    NX_DEBUG(this, "%1 LED %2", (state == LedState::enabled ? "Enabling" : "Disabling"), ledId);
    return m_platformAbstraction->setLedState(ledId, state);
}

} // namespace nx::vms::server::nvr::hanwha
