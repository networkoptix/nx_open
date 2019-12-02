#include "network_block_state_fetcher.h"

#include <nx/utils/elapsed_timer.h>

#include <nx/vms/server/nvr/hanwha/network_block/i_network_block_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

using namespace nx::vms::api;

static const std::chrono::milliseconds kSleepBetweenIterations(1000);

NetworkBlockStateFetcher::NetworkBlockStateFetcher(
    INetworkBlockPlatformAbstraction* platformAbstraction,
    StateHandler stateHandler)
    :
    m_platformAbstraction(platformAbstraction),
    m_stateHandler(std::move(stateHandler))
{
    NX_DEBUG(this, "Creating network block state fetcher");
    setObjectName("NetworkBlockStateFetcher");
}

NetworkBlockStateFetcher::~NetworkBlockStateFetcher()
{
    NX_DEBUG(this, "Destroying network block state fetcher");
    stop();
}

void NetworkBlockStateFetcher::stop()
{
    QnLongRunnable::pleaseStop();

    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        m_platformAbstraction->interrupt();
        m_waitCondition.wakeOne();
    }

    QnLongRunnable::wait();
}

void NetworkBlockStateFetcher::run()
{
    NX_ASSERT(m_stateHandler);

    const int portCount = m_platformAbstraction->portCount();
    while (!needToStop())
    {
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            nx::utils::ElapsedTimer timer;
            timer.restart();
            while (!needToStop() && timer.elapsed() < kSleepBetweenIterations)
            {
                m_waitCondition.wait(
                    &m_mutex,
                    std::max(
                        std::chrono::milliseconds::zero(),
                        kSleepBetweenIterations - timer.elapsed()));

                if (needToStop())
                    return;
            }
        }
        NetworkPortStateList result;
        for (int portNumber = 1; portNumber <= portCount; ++portNumber)
        {
            if (needToStop())
                return;

            result.push_back(m_platformAbstraction->portState(portNumber));
        }

        m_stateHandler(result);
    }
}

} // namespace nx::vms::server::nvr::hanwha
