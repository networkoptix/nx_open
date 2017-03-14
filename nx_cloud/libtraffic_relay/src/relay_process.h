#pragma once

#include <nx/utils/std/future.h>

#include <utils/common/stoppable.h>

namespace nx {
namespace cloud {
namespace relay {

class RelayProcess:
    public QnStoppable
{
public:
    RelayProcess(int argc, char **argv);
    virtual ~RelayProcess();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> handler);

    int exec();

private:
    int m_argc;
    char** m_argv;
    nx::utils::promise<void> m_processTerminationEvent;
    nx::utils::MoveOnlyFunc<void(bool /*isStarted*/)> m_startedEventHandler;
};

} // namespace relay
} // namespace cloud
} // namespace nx
