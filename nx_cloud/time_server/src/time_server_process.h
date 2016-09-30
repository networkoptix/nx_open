#pragma once

#include <nx/utils/std/future.h>
#include <utils/common/stoppable.h>

namespace nx {
namespace time_server {

class TimeServerProcess:
    public QnStoppable
{
public:
    TimeServerProcess(int argc, char **argv);
    virtual ~TimeServerProcess();

    virtual void pleaseStop() override;

    void setOnStartedEventHandler(
        nx::utils::MoveOnlyFunc<void(bool /*result*/)> handler);

    int exec();

private:
    int m_argc;
    char** m_argv;
    nx::utils::promise<void> m_processTerminationEvent;
    nx::utils::MoveOnlyFunc<void(bool /*result*/)> m_startedEventHandler;
};

} // namespace time_server
} // namespace nx
