#include "io_state_fetcher.h"

#include <chrono>

#include <utils/common/synctime.h>

#include <nx/vms/server/nvr/hanwha/utils.h>
#include <nx/vms/server/nvr/hanwha/common.h>
#include <nx/vms/server/nvr/hanwha/io/i_io_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

static const std::chrono::milliseconds kSleepBetweenIterations(10);

IoStateFetcher::IoStateFetcher(
    IIoPlatformAbstraction* platformAbstraction,
    StateHandler stateHandler)
    :
    m_platformAbstraction(platformAbstraction),
    m_stateHandler(std::move(stateHandler))
{
}

IoStateFetcher::~IoStateFetcher()
{
    stop();
}

void IoStateFetcher::run()
{
    NX_ASSERT(m_stateHandler);

    while (!needToStop())
    {
        std::this_thread::sleep_for(kSleepBetweenIterations);

        std::set<QnIOStateData> portStates;
        for (int i = 0; i < kInputCount; ++i)
            portStates.insert(m_platformAbstraction->portState(makeInputId(i)));

        m_stateHandler(portStates);
    }
}

} // namespace nx::vms::server::nvr::hanwha
