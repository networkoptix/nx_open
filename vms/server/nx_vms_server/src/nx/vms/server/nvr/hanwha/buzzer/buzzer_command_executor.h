#pragma once

#include <nx/utils/thread/wait_condition.h>
#include <nx/utils/thread/long_runnable.h>

#include <nx/vms/server/nvr/types.h>

namespace nx::vms::server::nvr::hanwha {

class IBuzzerPlatformAbstraction;

class BuzzerCommandExecutor: public QnLongRunnable
{
    using base_type = QnLongRunnable;
public:
    BuzzerCommandExecutor(IBuzzerPlatformAbstraction* platformAbstraction);

    virtual ~BuzzerCommandExecutor() override;

    void setState(BuzzerState state);

    virtual void stop() override;

protected:
    virtual void run() override;

private:
    nx::utils::Mutex m_mutex;
    nx::utils::WaitCondition m_waitCondition;

    BuzzerState m_state = BuzzerState::undefined;
    BuzzerState m_previousState = BuzzerState::undefined;

    IBuzzerPlatformAbstraction* m_platformAbstraction;
};

} // namespace nx::vms::server::nvr::hanwha
