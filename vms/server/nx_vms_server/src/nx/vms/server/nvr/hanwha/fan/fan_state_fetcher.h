#pragma once

#include <functional>

#include <nx/utils/thread/long_runnable.h>
#include <nx/utils/thread/wait_condition.h>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class IFanPlatformAbstraction;

class FanStateFetcher: public QnLongRunnable
{
public:
    using StateHandler = std::function<void(FanState state)>;

public:
    FanStateFetcher(
        IFanPlatformAbstraction* platformAbstraction,
        StateHandler stateHandler);

    virtual ~FanStateFetcher();

protected:
    virtual void run() override;

private:
    mutable nx::utils::Mutex m_mutex;
    mutable nx::utils::WaitCondition m_waitCondition;

    IFanPlatformAbstraction* m_platformAbstraction = nullptr;
    StateHandler m_stateHandler;
};


} // namespace nx::vms::server::nvr::hanwha
