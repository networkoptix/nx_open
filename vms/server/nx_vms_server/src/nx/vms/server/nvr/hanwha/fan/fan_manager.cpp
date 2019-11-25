#include "fan_manager.h"

#include <nx/utils/log/log.h>

#include <nx/vms/server/nvr/hanwha/utils.h>
#include <nx/vms/server/nvr/hanwha/fan/fan_state_fetcher.h>
#include <nx/vms/server/nvr/hanwha/fan/i_fan_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

FanManager::FanManager(std::unique_ptr<IFanPlatformAbstraction> platformAbstraction):
    m_platformAbstraction(std::move(platformAbstraction)),
    m_stateFetcher(std::make_unique<FanStateFetcher>(
        m_platformAbstraction.get(),
        [this](FanState state) { updateState(state); }))
{
    NX_DEBUG(this, "Creating the fan state manager");
}

FanManager::~FanManager()
{
    NX_DEBUG(this, "Destroying the fan state manager");
    stop();
}

void FanManager::start()
{
    NX_DEBUG(this, "Starting the fan state manager");
    m_stateFetcher->start();
}

void FanManager::stop()
{
    if (m_isStopped)
        return;

    NX_DEBUG(this, "Stopping the fan manager");
    m_stateFetcher->stop();
    m_isStopped = true;
}

FanState FanManager::state() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_lastState;
}

HandlerId FanManager::registerStateChangeHandler(StateChangeHandler handler)
{
    NX_MUTEX_LOCKER lock(&m_handlerMutex);
    NX_DEBUG(this, "Registering fan state change handler, id: %1", ++m_maxHandlerId);

    m_handlers.emplace(m_maxHandlerId, std::move(handler));
    return m_maxHandlerId;
}

void FanManager::unregisterStateChangeHandler(HandlerId handlerId)
{
    NX_DEBUG(this, "Unregistering fan state change handler, id %1", handlerId);

    NX_MUTEX_LOCKER lock(&m_handlerMutex);
    if (const auto it = m_handlers.find(handlerId); it != m_handlers.cend())
    {
        m_handlers.erase(handlerId);
    }
    else
    {
        NX_DEBUG(this, "Handler with id %1 doesn't exist");
    }

}

void FanManager::updateState(FanState state)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (state == m_lastState)
        return;

    NX_DEBUG(this, "Fan state changed from %1 to %2, calling the handlers", m_lastState, state);

    m_lastState = state;
    for (const auto& [_, handler]: m_handlers)
        handler(state);
}

} // namespace nx::vms::server::nvr::hanwha
