#pragma once

#include <set>
#include <functional>

#include <api/model/api_ioport_data.h>

#include <nx/utils/thread/long_runnable.h>

namespace nx::vms::server::nvr::hanwha {

class IIoPlatformAbstraction;

class IoStateFetcher: public QnLongRunnable
{
public:
    using StateHandler = std::function<void(const std::set<QnIOStateData>&)>;
public:
    IoStateFetcher(
        IIoPlatformAbstraction* platformAbstraction,
        StateHandler stateHandler);

    virtual ~IoStateFetcher();

protected:
    virtual void run() override;

private:
    IIoPlatformAbstraction* m_platformAbstraction = nullptr;
    StateHandler m_stateHandler;
};

} // namespace nx::vms::server::nvr::hanwha
