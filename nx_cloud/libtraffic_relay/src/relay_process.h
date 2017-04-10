#pragma once

#include <nx/utils/move_only_func.h>
#include <nx/utils/std/future.h>
#include <nx/utils/thread/stoppable.h>

namespace nx {
namespace cloud {
namespace relay {

namespace conf { class Settings; }

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

    void initializeLog(const conf::Settings& settings);
};

} // namespace relay
} // namespace cloud
} // namespace nx
