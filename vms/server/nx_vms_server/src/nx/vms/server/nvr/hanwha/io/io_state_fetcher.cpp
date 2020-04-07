#include "io_state_fetcher.h"

#include <chrono>

#include <utils/common/synctime.h>

#include <nx/vms/server/nvr/hanwha/utils.h>
#include <nx/vms/server/nvr/hanwha/common.h>
#include <nx/vms/server/nvr/hanwha/hanwha_nvr_ini.h>
#include <nx/vms/server/nvr/hanwha/io/i_io_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

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

void IoStateFetcher::updateOutputPortState(const QString& portId, IoPortState state)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    NX_DEBUG(this, "Updating output port state, port id %1, state: %2", portId, state);
    m_outputPortStates[portId] = state;
}

void IoStateFetcher::run()
{
    NX_ASSERT(m_stateHandler);

    while (!needToStop())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(nvrIni().ioStatePollingIntervalMs));

        std::set<QnIOStateData> portStateData;
        {
            NX_MUTEX_LOCKER lock(&m_mutex);
            const int64_t currentTimestampUs = qnSyncTime->currentUSecsSinceEpoch();

            for (const auto& [portId, portState]: m_outputPortStates)
            {
                QnIOStateData outputPortStateData;
                outputPortStateData.id = portId;
                outputPortStateData.isActive = (portState == IoPortState::active);
                outputPortStateData.timestamp = currentTimestampUs;
                portStateData.insert(outputPortStateData);
            }
        }

        for (int i = 0; i < kInputCount; ++i)
        {
            if (needToStop())
                return;

            portStateData.insert(m_platformAbstraction->portState(makeInputId(i)));
        }

        m_stateHandler(portStateData);
    }
}

} // namespace nx::vms::server::nvr::hanwha
