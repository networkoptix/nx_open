#include "fan_state_fetcher.h"

#include <chrono>

#include <nx/utils/log/log.h>

#include <nx/vms/server/nvr/hanwha/fan/i_fan_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

FanStateFetcher::FanStateFetcher(
    IFanPlatformAbstraction* platformAbstraction,
    StateHandler stateHandler)
    :
    m_platformAbstraction(platformAbstraction),
    m_stateHandler(std::move(stateHandler))
{
    NX_DEBUG(this, "Creating the fan state fetcher");
    setObjectName("FanStateFetcher");
}

FanStateFetcher::~FanStateFetcher()
{
    NX_DEBUG(this, "Destroying the fan state fetcher");
    pleaseStop();
    m_platformAbstraction->interrupt();
    m_waitCondition.wakeOne();
    wait();
}

void FanStateFetcher::run()
{
    static const std::chrono::seconds kTimeoutBetweenIterations(1);

    NX_DEBUG(this, "Starting the fan state fetcher thread");
    while (!needToStop())
    {
        m_stateHandler(m_platformAbstraction->state());

        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            m_waitCondition.wait(&m_mutex, kTimeoutBetweenIterations);
        }
    }
}

} // namespace nx::vms::server::nvr::hanwha
