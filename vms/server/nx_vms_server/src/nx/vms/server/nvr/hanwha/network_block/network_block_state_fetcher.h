#pragma once

#include <functional>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/utils/thread/long_runnable.h>

#include <nx/vms/server/nvr/hanwha/network_block/i_network_block_platform_abstraction.h>

namespace nx::vms::server::nvr::hanwha {

class NetworkBlockStateFetcher: public QnLongRunnable
{
public:
    using StateHandler = std::function<void(const NetworkPortStateList& state)>;

public:
    NetworkBlockStateFetcher(
        INetworkBlockPlatformAbstraction* platformAbstraction,
        StateHandler stateHandler);

    virtual ~NetworkBlockStateFetcher();

    virtual void stop() override;

protected:
    virtual void run() override;

private:
    nx::utils::Mutex m_mutex;
    nx::utils::WaitCondition m_waitCondition;

    INetworkBlockPlatformAbstraction* m_platformAbstraction = nullptr;
    StateHandler m_stateHandler;
};

} // namespace nx::vms::server::nvr::hanwha
