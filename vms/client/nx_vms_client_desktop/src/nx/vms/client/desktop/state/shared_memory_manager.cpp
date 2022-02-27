// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "shared_memory_manager.h"

#include <chrono>
#include <functional>

#include <QtCore/QTimer>

#include <nx/reflect/json.h>
#include <nx/reflect/to_string.h>
#include <nx/utils/log/assert.h>

void serialize(
    nx::reflect::json::SerializationContext* ctx,
    const nx::vms::client::desktop::SharedMemoryData::ScreenUsageData& data)
{
    ctx->composer.startArray();
    for (const auto& item: data)
        ctx->composer.writeInt(item);
    ctx->composer.endArray();
}

namespace nx::vms::client::desktop {

using Command = SharedMemoryData::Command;
using ScreenUsageData = SharedMemoryData::ScreenUsageData;

namespace {

using namespace std::chrono;
static constexpr auto kWatcherInterval = 250ms;

using DataFilter = std::function<bool(const SharedMemoryData::Process&)>;

DataFilter pidEqualTo(SharedMemoryData::PidType value)
{
    return [value](const SharedMemoryData::Process& data) { return data.pid == value; };
}

DataFilter sessionIdEqualTo(SessionId value)
{
    return [value](const SharedMemoryData::Process& data) { return data.sessionId == value; };
}

} // namespace

void PrintTo(SharedMemoryData::Command command, ::std::ostream* os)
{
    *os << nx::reflect::toString(command);
}

void PrintTo(SharedMemoryData::Process process, ::std::ostream* os)
{
    *os << nx::reflect::json::serialize(process);
}

void PrintTo(SharedMemoryData::Session session, ::std::ostream* os)
{
    *os << nx::reflect::json::serialize(session);
}

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

    static auto findProcess(auto& data, const auto& predicate)
    {
        auto process = std::find_if(data.processes.begin(), data.processes.end(), predicate);
        return process != data.processes.end() ? &(*process) : nullptr;
    }

    auto findCurrentProcess(auto& data) const
    {
        return findProcess(data, pidEqualTo(currentProcessPid));
    }

    /** Cleanup data, which processes are closed already. */
    void cleanDeadInstances()
    {
        SharedMemoryLocker guard(memoryInterface);
        SharedMemoryData data = memoryInterface->data();
        for (auto& process: data.processes)
        {
            if (process.pid != 0
                && process.pid != currentProcessPid
                && !processInterface->isProcessRunning(process.pid))
            {
                process = {};
            }
        }
        for (auto& session: data.sessions)
        {
            const bool sessionIsActive = findProcess(data, sessionIdEqualTo(session.id));
            if (!sessionIsActive)
                session = {};
        }
        memoryInterface->setData(data);
    }

    void registerCurrentInstance()
    {
        SharedMemoryLocker guard(memoryInterface);
        SharedMemoryData data = memoryInterface->data();
        auto existing = findCurrentProcess(data);

        if (!NX_ASSERT(!existing, "Current instance is already registered"))
            return;

        auto empty = findProcess(data, pidEqualTo(0));
        if (!NX_ASSERT(empty, "No space for the current instance"))
            return;

        *empty = {.pid = currentProcessPid};
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

        SessionId sessionId;
        auto instance = findCurrentProcess(data);
        if (NX_ASSERT(instance, "Current instance is unregistered"))
            sessionId = instance->sessionId;

        // Send this command only if we are not logged in.
        if (sessionId != SessionId())
        {
            sendCommandUnderLock(
                Command::saveWindowState,
                [&](const SharedMemoryData::Process& block)
                {
                    return block.sessionId == sessionId;
                });
        }
    }

    void assignStatesToOtherInstances(QStringList* windowStates)
    {
        SharedMemoryLocker guard(memoryInterface);

        SharedMemoryData data = memoryInterface->data();
        bool modified = false;

        SessionId sessionId;
        auto instance = findCurrentProcess(data);
        if (NX_ASSERT(instance, "Current instance is unregistered"))
            sessionId = instance->sessionId;

        if (!NX_ASSERT(sessionId != SessionId(), "Current instance is not logged in"))
            return;

        for (auto& memoryBlock: data.processes)
        {
            if (windowStates->empty())
                break;

            if (memoryBlock.pid != 0
                && memoryBlock.pid != currentProcessPid
                && processInterface->isProcessRunning(memoryBlock.pid)
                && memoryBlock.sessionId == sessionId)
            {
                memoryBlock.command = Command::restoreWindowState;
                memoryBlock.commandData = windowStates->takeFirst().toLatin1() + '\0';
                modified = true;
            }
        }

        if (modified)
            memoryInterface->setData(data);
    }

    void sendCommand(Command command, DataFilter filter = {})
    {
        SharedMemoryLocker guard(memoryInterface);
        sendCommandUnderLock(command, filter);
    }

    void sendCommandUnderLock(Command command, DataFilter filter = {})
    {
        SharedMemoryData data = memoryInterface->data();
        bool modified = false;

        for (auto& memoryBlock: data.processes)
        {
            if (memoryBlock.pid != 0 //< Do not write commands to uninitialized memory.
                && memoryBlock.pid != currentProcessPid //< Do not send commands to self.
                && (!filter || filter(memoryBlock)))
            {
                // Do not override existing commands with higher priority.
                if (memoryBlock.command < command)
                {
                    memoryBlock.command = command;
                    modified = true;
                }
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

    bool sessionHasOtherProcesses(auto data, const SessionId& sessionId) const
    {
        return findProcess(
            data,
            [&](const SharedMemoryData::Process& process)
            {
                return process.pid != currentProcessPid && process.sessionId == sessionId;
            });
    }

    bool isLastInstanceInCurrentSession() const
    {
        SharedMemoryLocker guard(memoryInterface);
        const SharedMemoryData data = memoryInterface->data();

        auto process = findCurrentProcess(data);
        if (!process || process->sessionId == SessionId())
            return false;

        return !std::any_of(data.processes.cbegin(), data.processes.cend(),
            [this, sessionId = process->sessionId](
                const SharedMemoryData::Process& process)
            {
                return process.pid != currentProcessPid
                    && process.sessionId == sessionId
                    && processInterface->isProcessRunning(process.pid);
            });
    }

    auto findSession(auto& data, const SessionId& sessionId)
    {
        auto session = std::find_if(data.sessions.begin(), data.sessions.end(),
            [&](const SharedMemoryData::Session& session) { return session.id == sessionId; });

        return session != data.sessions.end() ? &(*session) : nullptr;
    }

    auto findCurrentSession(auto& data)
    {
        auto process = findCurrentProcess(data);
        return process ? findSession(data, process->sessionId) : nullptr;
    }

    void addSession(SharedMemoryData& data, const SessionId& sessionId)
    {
        auto empty = findSession(data, {});
        if (!NX_ASSERT(empty, "No space for new session"))
            return;

        *empty = {.id = sessionId};
    }

    void enterSession(const SessionId& sessionId)
    {
        SharedMemoryLocker guard(memoryInterface);
        auto data = memoryInterface->data();

        if (!findSession(data, sessionId))
            addSession(data, sessionId);

        auto process = findCurrentProcess(data);
        if (!NX_ASSERT(process, "Current process is not registered"))
            return;

        process->sessionId = sessionId;

        memoryInterface->setData(data);
    }

    bool leaveSession()
    {
        SharedMemoryLocker guard(memoryInterface);
        auto data = memoryInterface->data();
        auto process = findCurrentProcess(data);
        if (!NX_ASSERT(process && process->sessionId != SessionId(), "Process is not registered"))
            return false;

        const auto sessionId = std::exchange(process->sessionId, {});
        auto session = findSession(data, sessionId);
        if (!NX_ASSERT(session, "Session is not registered"))
            return false;

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
        auto session = findCurrentSession(data);

        if (!NX_ASSERT(session, "Session is not registered"))
            return;

        session->token = token;
        memoryInterface->setData(data);
        this->lastSessionToken = token;
    }

    SharedMemoryInterfacePtr memoryInterface;
    ClientProcessExecutionInterfacePtr processInterface;
    const ClientProcessExecutionInterface::PidType currentProcessPid;
    QTimer watcher;
    std::string lastSessionToken;
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

QList<int> SharedMemoryManager::runningInstancesIndices() const
{
    const SharedMemoryData data = d->readData();

    QList<int> result;
    for (int i = 0; i < data.processes.size(); ++i)
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
    if (auto session = d->findCurrentSession(data))
    {
        if (d->lastSessionToken != session->token)
        {
            d->lastSessionToken = session->token;
            if (!d->lastSessionToken.empty())
                emit sessionTokenChanged(d->lastSessionToken);
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

} // namespace nx::vms::client::desktop
