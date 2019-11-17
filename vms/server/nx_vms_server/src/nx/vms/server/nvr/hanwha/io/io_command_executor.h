#pragma once

#include <map>

#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/long_runnable.h>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class IIoPlatformAbstraction;

class IoCommandExecutor: public QnLongRunnable
{
    using base_type = QnLongRunnable;
public:
    IoCommandExecutor(
        IIoPlatformAbstraction* platformAbstraction,
        IoStateChangeHandler stateChangeHandler);

    virtual ~IoCommandExecutor() override;

    void setOutputPortState(const QString& portId, IoPortState state);

    virtual void stop() override;

protected:
    virtual void run() override;

private:
    bool hasChangedPortStates() const;

private:
    nx::utils::Mutex m_mutex;
    nx::utils::WaitCondition m_waitCondition;

    struct IoPortContext
    {
        IoPortState state = IoPortState::undefined;
        IoPortState previousState = IoPortState::undefined;
    };

    std::map</*portId*/ QString, IoPortContext> m_portStates;

    IIoPlatformAbstraction* m_platformAbstraction;
    IoStateChangeHandler m_stateChangeHandler;
};

} // namespace nx::vms::server::nvr::hanwha
