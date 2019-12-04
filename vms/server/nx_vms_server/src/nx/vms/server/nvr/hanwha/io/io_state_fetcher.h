#pragma once

#include <set>
#include <map>
#include <functional>

#include <api/model/api_ioport_data.h>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/long_runnable.h>

#include <nx/vms/server/nvr/types.h>

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

    void updateOutputPortState(const QString& portId, IoPortState state);

protected:
    virtual void run() override;

private:
    mutable nx::utils::Mutex m_mutex;

    IIoPlatformAbstraction* m_platformAbstraction = nullptr;
    StateHandler m_stateHandler;
    std::map</*outputPortId*/ QString, IoPortState> m_outputPortStates;
};

} // namespace nx::vms::server::nvr::hanwha
