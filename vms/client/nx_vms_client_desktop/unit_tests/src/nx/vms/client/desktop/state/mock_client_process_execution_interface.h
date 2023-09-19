// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QSet>

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/state/client_process_execution_interface.h>
#include <nx/vms/client/desktop/state/startup_parameters.h>

namespace nx::vms::client::desktop {
namespace test {

using PidType = ClientProcessExecutionInterface::PidType;
using RunningProcessesMapType = QMap<PidType, StartupParameters>;
using RunningProcessesMap = std::shared_ptr<RunningProcessesMapType>;

static inline RunningProcessesMap makeMockProcessesMap()
{
    return std::make_shared<RunningProcessesMapType>();
}

class MockClientProcessExecutionInterface: public ClientProcessExecutionInterface
{
public:
    static constexpr PidType kCurrentProcessPid = 1;

    MockClientProcessExecutionInterface(RunningProcessesMap processesMap):
        m_processesMap(processesMap)
    {
        NX_ASSERT(processesMap);
        m_processesMap->insert(kCurrentProcessPid, {});
    }

    PidType givenRunningProcess(const SessionId& sessionId = SessionId())
    {
        StartupParameters parameters{
            StartupParameters::Mode::loadSession,
            sessionId,
            QString()};
        return runClient(parameters);
    }

    RunningProcessesMapType runningProcesses() const
    {
        return *m_processesMap;
    }

    void whenProcessIsKilled(PidType pid)
    {
        NX_ASSERT(isProcessRunning(pid));
        m_processesMap->remove(pid);
    }

    virtual PidType currentProcessPid() const override
    {
        return kCurrentProcessPid;
    }

    virtual bool isProcessRunning(PidType pid) const override
    {
        return m_processesMap->contains(pid);
    }

    virtual PidType runClient(const QStringList& arguments) const override
    {
        NX_ASSERT(false, "A mock for runClient(const QStringList& arguments) is not implemented");
        return 0;
    }

    virtual PidType runClient(const StartupParameters& parameters) const override
    {
        NX_ASSERT(!m_processesMap->isEmpty());
        auto lastElement = m_processesMap->constKeyValueEnd();
        --lastElement;

        PidType freePid = (*lastElement).first + 1;
        m_processesMap->insert(freePid, parameters);
        return freePid;
    }

private:
    RunningProcessesMap m_processesMap;
};

} // namespace test
} // namespace nx::vms::client::desktop
