// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_memory_manager.h"

#include <chrono>
#include <functional>

#include <QtCore/QTimer>

#include <nx/utils/log/log.h>
#include <nx/vms/client/core/network/cloud_status_watcher.h>
#include <nx/vms/client/desktop/application_context.h>

namespace nx::vms::client::desktop {

using Command = SharedMemoryData::Command;
using ScreenUsageData = SharedMemoryData::ScreenUsageData;
using namespace shared_memory;

namespace {

using namespace std::chrono;
static constexpr auto kWatcherInterval = 250ms;

} // namespace

struct SharedMemoryLocker
{
    SharedMemoryLocker(const SharedMemoryInterfacePtr& memoryInterface):
        m_memoryInterface(memoryInterface.get())
    {
        m_memoryInterface->lock();
    }

    ~SharedMemoryLocker()
    {
        m_memoryInterface->unlock();
    }

private:
    SharedMemoryInterface* const m_memoryInterface;
};

struct SharedMemoryManager::Private
{
    Private(
        SharedMemoryInterfacePtr memoryInterface,
        ClientProcessExecutionInterfacePtr processInterface)
        :
        memoryInterface(std::move(memoryInterface)),
        processInterface(std::move(processInterface)),
        currentProcessPid(this->processInterface->currentProcessPid())
    {
    }

    auto* findCurrentProcess(auto& data) const
    {
        return data.findProcess(currentProcessPid);
    }

    auto isRunning(bool isRunning) const
    {
        return exceptPids({0}) | ranges::views::filter(
            [this, isRunning](const SharedMemoryData::Process& process)
            {
                return processInterface->isProcessRunning(process.pid) == isRunning;
            });
    }

    auto otherWithinSession(SessionId sessionId) const
    {
        return withinSession(sessionId) | exceptPids({currentProcessPid});
    }

    /** Cleanup data, which processes are closed already. */
    void cleanDeadInstances()
    {
        SharedMemoryLocker guard(memoryInterface);
        SharedMemoryData data = memoryInterface->data();

        for (auto& process: data.processes | isRunning(false) | exceptPids({currentProcessPid}))
            process = {};

        for (auto& session: data.sessions)
        {
            const bool sessionIsActive = !(data.processes | withinSession(session.id)).empty();
            if (!sessionIsActive)
                session = {};
        }
        memoryInterface->setData(data);
    }

    void registerCurrentInstance()
    {
        SharedMemoryLocker guard(memoryInterface);
        SharedMemoryData data = memoryInterface->data();
        if (data.addProcess(currentProcessPid))
            memoryInterface->setData(data);
    }

    void updateCurrentInstance(std::function<void(SharedMemoryData::Process*)> transform)
    {
        SharedMemoryLocker guard(memoryInterface);
        SharedMemoryData data = memoryInterface->data();
        auto existing = findCurrentProcess(data);

        if (!NX_ASSERT(existing, "Current instance is unregistered"))
            return;

        transform(existing);
        memoryInterface->setData(data);
    }

    void unregisterCurrentInstance()
    {
        updateCurrentInstance([](auto iter) { *iter = SharedMemoryData::Process(); });
    }

    SharedMemoryData readData() const
    {
        SharedMemoryLocker guard(memoryInterface);
        return memoryInterface->data();
    }

    void requestToSaveState()
    {
        SharedMemoryLocker guard(memoryInterface);
        const SharedMemoryData data = memoryInterface->data();
        auto session = data.findProcessSession(currentProcessPid);
        if (NX_ASSERT(session, "Session not found"))
            sendCommandUnderLock(Command::saveWindowState, withinSession(session->id));
    }

    void assignStatesToOtherInstances(QStringList* windowStates)
    {
        SharedMemoryLocker guard(memoryInterface);
        SharedMemoryData data = memoryInterface->data();
        bool modified = false;

        auto session = data.findProcessSession(currentProcessPid);
        if (!NX_ASSERT(session, "Session not found"))
            return;

        for (auto& process: data.processes | isRunning(true) | otherWithinSession(session->id))
        {
            if (windowStates->empty())
                break;

            process.command = Command::restoreWindowState;
            process.commandData = windowStates->takeFirst().toLatin1() + '\0';
            modified = true;
        }

        if (modified)
            memoryInterface->setData(data);
    }

    void sendCommand(Command command, PipeFilterType filter = all())
    {
        SharedMemoryLocker guard(memoryInterface);
        sendCommandUnderLock(command, filter);
    }

    void sendCommandUnderLock(Command command, PipeFilterType filter = all())
    {
        SharedMemoryData data = memoryInterface->data();
        bool modified = false;

        for (auto& process: data.processes | exceptPids({currentProcessPid, 0}) | filter)
        {
            // Do not override existing commands with higher priority.
            if (process.command < command)
            {
                process.command = command;
                modified = true;
            }
        }

        if (modified)
            memoryInterface->setData(data);
    }

    std::pair<Command, QByteArray> takeCommand()
    {
        SharedMemoryLocker guard(memoryInterface);
        SharedMemoryData data = memoryInterface->data();
        auto instance = findCurrentProcess(data);
        if (NX_ASSERT(instance, "Current instance is unregistered"))
        {
            auto command = instance->command;
            if (command != Command::none)
            {
                auto commandData = instance->commandData;
                instance->command = Command::none;
                instance->commandData.clear();
                memoryInterface->setData(data);
                return {command, commandData};
            }
        }

        return {Command::none, {}};
    }

    bool isLastInstanceInCurrentSession() const
    {
        SharedMemoryLocker guard(memoryInterface);
        const SharedMemoryData data = memoryInterface->data();

        auto session = data.findProcessSession(currentProcessPid);
        if (!session)
            return false;

        auto instances = data.processes | isRunning(true) | otherWithinSession(session->id);
        return !instances.empty();
    }

    bool sessionHasOtherProcesses(const SharedMemoryData& data, SessionId sessionId) const
    {
        auto instances = data.processes | otherWithinSession(sessionId);
        return !instances.empty();
    }

    void enterSession(const SessionId& sessionId)
    {
        SharedMemoryLocker guard(memoryInterface);
        auto data = memoryInterface->data();

        if (!data.findSession(sessionId))
            data.addSession(sessionId);

        auto process = data.findProcess(currentProcessPid);
        if (!NX_ASSERT(process, "Current process is not registered"))
            return;

        NX_DEBUG(this, "Process %1 enters session %2",
            process->pid, process->sessionId.toLogString());
        process->sessionId = sessionId;

        memoryInterface->setData(data);
    }

    bool leaveSession()
    {
        SharedMemoryLocker guard(memoryInterface);
        auto data = memoryInterface->data();
        auto process = findCurrentProcess(data);
        if (!NX_ASSERT(process, "Process is not registered"))
            return false;

        // In case of race condition current session can be already unregistered.
        if (!process->sessionId)
        {
            NX_WARNING(this, "Current process session is already unregistered.");
            return false;
        }

        const auto sessionId = std::exchange(process->sessionId, {});
        auto session = data.findSession(sessionId);
        if (!NX_ASSERT(session, "Session data inconsistency"))
            return false;

        NX_DEBUG(this, "Process %1 leaves session %2",
            process->pid, process->sessionId.toLogString());

        const bool sessionIsStillActive = sessionHasOtherProcesses(data, sessionId);
        if (!sessionIsStillActive)
            *session = {};

        memoryInterface->setData(data);
        return sessionIsStillActive;
    }

    void updateSessionToken(std::string token)
    {
        SharedMemoryLocker guard(memoryInterface);
        auto data = memoryInterface->data();
        auto session = data.findProcessSession(currentProcessPid);
        if (!NX_ASSERT(session, "Session is not registered"))
            return;

        session->token = token;
        memoryInterface->setData(data);
        this->lastSessionToken = token;
    }

    void updateRefreshToken(std::string refreshToken)
    {
        SharedMemoryLocker guard(memoryInterface);
        auto data = memoryInterface->data();
        auto cloudUserSession = data.findProcessCloudUserSession(currentProcessPid);
        if (!NX_ASSERT(cloudUserSession, "Cloud user is not registered in shared memory"))
            return;

        if (cloudUserSession->refreshToken == refreshToken)
            return;

        cloudUserSession->refreshToken = refreshToken;
        memoryInterface->setData(data);
        this->lastRefreshToken = refreshToken;
    }

    void enterCloudUserSession(const SharedMemoryData::CloudUserName& cloudUserName)
    {
        SharedMemoryLocker guard(memoryInterface);
        auto data = memoryInterface->data();

        if (!data.findCloudUserSession(cloudUserName))
            data.addCloudUserSession(cloudUserName);

        auto process = data.findProcess(currentProcessPid);
        if (!NX_ASSERT(process, "Current process is not registered"))
            return;

        NX_DEBUG(this, "Process %1 enters cloud user %2", process->pid, process->cloudUserName);
        process->cloudUserName = cloudUserName;

        memoryInterface->setData(data);
    }

    void leaveCloudUserSession()
    {
        SharedMemoryLocker guard(memoryInterface);
        auto data = memoryInterface->data();
        auto process = findCurrentProcess(data);
        if (!NX_ASSERT(process, "Process is not registered"))
            return;

        // In case of race condition current session can be already unregistered.
        if (process->cloudUserName.empty())
        {
            NX_WARNING(this, "Current process session is already unregistered.");
            return;
        }
        const auto cloudUserName = std::exchange(process->cloudUserName, {});
        auto cloudUserSession = data.findCloudUserSession(cloudUserName);
        if (!cloudUserSession)
        {
            NX_DEBUG(this, "Cloud user session has already been deleted");
            return;
        }

        NX_DEBUG(
            this, "Process %1 leaves cloud user session %2", process->pid, process->cloudUserName);
        *cloudUserSession = {};

        memoryInterface->setData(data);
    }

    SharedMemoryInterfacePtr memoryInterface;
    ClientProcessExecutionInterfacePtr processInterface;
    const ClientProcessExecutionInterface::PidType currentProcessPid;
    QTimer watcher;
    std::string lastSessionToken;
    std::string lastRefreshToken;
};

SharedMemoryManager::SharedMemoryManager(
    SharedMemoryInterfacePtr memoryInterface,
    ClientProcessExecutionInterfacePtr processInterface,
    QObject* parent)
    :
    base_type(parent),
    d(new Private(std::move(memoryInterface), std::move(processInterface)))
{
    d->cleanDeadInstances();
    d->registerCurrentInstance();

    connect(&d->watcher, &QTimer::timeout, this, &SharedMemoryManager::processEvents);
    d->watcher.start(kWatcherInterval);
}

SharedMemoryManager::~SharedMemoryManager()
{
    d->unregisterCurrentInstance();
}

void SharedMemoryManager::connectToCloudStatusWatcher()
{
    if (auto cloudStatusWatcher = appContext()->cloudStatusWatcher())
    {
        connect(cloudStatusWatcher,
            &nx::vms::client::core::CloudStatusWatcher::refreshTokenChanged,
            this,
            &SharedMemoryManager::updateRefreshToken);

        connect(cloudStatusWatcher,
            &nx::vms::client::core::CloudStatusWatcher::cloudLoginChanged,
            this,
            [this]()
            {
                const auto username = appContext()->cloudStatusWatcher()->credentials().username;
                if (username.empty())
                    d->leaveCloudUserSession();
                else
                    d->enterCloudUserSession(username);
            });

        connect(this,
            &SharedMemoryManager::refreshTokenChanged,
            cloudStatusWatcher,
            &nx::vms::client::core::CloudStatusWatcher::updateRefreshToken);
    }
}

int SharedMemoryManager::currentInstanceIndex() const
{
    const SharedMemoryData data = d->readData();
    for (int i = 0; i < data.processes.size(); ++i)
    {
        if (data.processes[i].pid == d->currentProcessPid)
            return i;
    }
    NX_ASSERT(false, "Current instance is not registered.");
    return -1;
}

bool SharedMemoryManager::isSingleInstance() const
{
    const SharedMemoryData data = d->readData();
    return std::none_of(data.processes.cbegin(), data.processes.cend(),
        [this](const auto& block) { return block.pid != 0 && block.pid != d->currentProcessPid; });
}

QList<int> SharedMemoryManager::runningInstancesIndices() const
{
    const SharedMemoryData data = d->readData();

    QList<int> result;
    for (int i = 0; i < (int) data.processes.size(); ++i)
    {
        if (data.processes[i].pid != 0)
            result.push_back(i);
    }
    return result;
}

ScreenUsageData SharedMemoryManager::allUsedScreens() const
{
    const SharedMemoryData data = d->readData();
    ScreenUsageData result;
    for (const auto& block: data.processes)
    {
        if (block.pid == 0)
            continue;

        result |= block.usedScreens;
    }
    return result;
}

void SharedMemoryManager::setCurrentInstanceScreens(const ScreenUsageData& screens)
{
    d->updateCurrentInstance([&](auto iter) { iter->usedScreens = screens; });
}

void SharedMemoryManager::requestToSaveState()
{
    d->requestToSaveState();
}

void SharedMemoryManager::requestToExit()
{
    d->sendCommand(Command::exit);
}

void SharedMemoryManager::assignStatesToOtherInstances(QStringList* windowStates)
{
    d->assignStatesToOtherInstances(windowStates);
}

void SharedMemoryManager::requestSettingsReload()
{
    d->sendCommand(Command::reloadSettings);
}

bool SharedMemoryManager::sessionIsRunningAlready(const SessionId& sessionId) const
{
    const SharedMemoryData data = d->readData();
    return d->sessionHasOtherProcesses(data, sessionId);
}

bool SharedMemoryManager::isLastInstanceInCurrentSession() const
{
    return d->isLastInstanceInCurrentSession();
}

void SharedMemoryManager::processEvents()
{
    auto [command, commandData] = d->takeCommand();
    if (command != SharedMemoryData::Command::none)
        emit clientCommandRequested(command, commandData);

    const auto data = d->readData();
    if (auto session = data.findProcessSession(d->currentProcessPid))
    {
        if (d->lastSessionToken != session->token)
        {
            d->lastSessionToken = session->token;
            if (!d->lastSessionToken.empty())
                emit sessionTokenChanged(d->lastSessionToken);
        }
    }

    if (auto cloudUserSession = data.findProcessCloudUserSession(d->currentProcessPid))
    {
        if (d->lastRefreshToken != cloudUserSession->refreshToken)
        {
            d->lastRefreshToken = cloudUserSession->refreshToken;
            if (!d->lastRefreshToken.empty())
                emit refreshTokenChanged(d->lastRefreshToken);
        }
    }
}

void SharedMemoryManager::enterSession(const SessionId& sessionId)
{
    d->enterSession(sessionId);
}

bool SharedMemoryManager::leaveSession()
{
    return d->leaveSession();
}

void SharedMemoryManager::updateSessionToken(std::string token)
{
    d->updateSessionToken(token);
}

void SharedMemoryManager::updateRefreshToken(std::string refreshToken)
{
    d->updateRefreshToken(refreshToken);
}

void SharedMemoryManager::requestLogoutFromCloud()
{
    d->sendCommand(Command::logoutFromCloud);
}

} // namespace nx::vms::client::desktop
