#pragma once

#include <functional>

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

protected:
    virtual void run() override;

private:
    INetworkBlockPlatformAbstraction* m_platformAbstraction = nullptr;
    StateHandler m_stateHandler;
};

} // namespace nx::vms::server::nvr::hanwha
