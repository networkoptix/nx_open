// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mock_shared_memory_interface.h"

namespace nx::vms::client::desktop {
namespace test {

namespace {

using DataFilter = std::function<bool(const SharedMemoryData::Process&)>;
DataFilter pidEqualTo(SharedMemoryData::PidType value)
{
    return [value](const SharedMemoryData::Process& data) { return data.pid == value; };
}

void addSessionIfNeeded(SharedMemoryDataPtr memory, SessionId sessionId)
{
    auto& sessions = memory->sessions;
    auto session = std::find_if(sessions.begin(), sessions.end(),
        [&](const SharedMemoryData::Session& session) { return session.id == sessionId; });

    if (session != sessions.end())
        return;

    session = std::find_if(sessions.begin(), sessions.end(),
        [&](const SharedMemoryData::Session& session) { return session.id == SessionId(); });

    *session = {.id = sessionId};
}

} // namespace

PidType MockSharedMemoryInterface::givenAnotherRunningInstance(
    SharedMemoryDataPtr memory,
    RunningProcessesMap processesMap,
    const SessionId& sessionId,
    const SharedMemoryData::ScreenUsageData& screens)
{
    // Find first free block.
    auto freeBlock = 
        std::find_if(memory->processes.begin(), memory->processes.end(), pidEqualTo(0));

    MockClientProcessExecutionInterface processInterface(processesMap);
    freeBlock->pid = processInterface.givenRunningProcess();
    freeBlock->sessionId = sessionId;
    freeBlock->usedScreens = screens;

    addSessionIfNeeded(memory, sessionId);

    return freeBlock->pid;
}

SharedMemoryData::Process MockSharedMemoryInterface::sharedMemoryBlock(
    SharedMemoryDataPtr memory,
    PidType processPid)
{
    auto currentBlock =
        std::find_if(memory->processes.begin(), memory->processes.end(), pidEqualTo(processPid));

    if (currentBlock != memory->processes.end())
        return *currentBlock;

    return {};
}

void MockSharedMemoryInterface::setSharedMemoryBlock(
    SharedMemoryDataPtr memory,
    SharedMemoryData::Process data,
    PidType processPid)
{
    data.pid = processPid;

    auto currentBlock =
        std::find_if(memory->processes.begin(), memory->processes.end(), pidEqualTo(processPid));

    if (currentBlock == memory->processes.end())
    {
        currentBlock =
            std::find_if(memory->processes.begin(), memory->processes.end(), pidEqualTo(0));
    }

    NX_ASSERT(currentBlock != memory->processes.end());
    *currentBlock = data;
}

} // namespace test
} // namespace nx::vms::client::desktop
