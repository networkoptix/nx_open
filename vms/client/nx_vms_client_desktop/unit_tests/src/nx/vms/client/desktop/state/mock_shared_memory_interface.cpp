// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "mock_shared_memory_interface.h"

namespace nx::vms::client::desktop {
namespace test {
namespace {

void addSessionIfNeeded(SharedMemoryDataPtr memory, SessionId sessionId)
{
    if (sessionId && !memory->findSession(sessionId))
        memory->addSession(sessionId);
}

SharedMemoryData::Process* addProcessIfNeeded(SharedMemoryDataPtr memory, PidType pid)
{
    auto process = memory->findProcess(pid);
    return process ? process : memory->addProcess(pid);
}

} // namespace

PidType MockSharedMemoryInterface::givenAnotherRunningInstance(
    SharedMemoryDataPtr memory,
    RunningProcessesMap processesMap,
    const SessionId& sessionId,
    const SharedMemoryData::ScreenUsageData& screens)
{
    MockClientProcessExecutionInterface processInterface(processesMap);
    auto freeBlock = memory->addProcess(processInterface.givenRunningProcess());
    freeBlock->usedScreens = screens;
    addSessionIfNeeded(memory, sessionId);
    freeBlock->sessionId = sessionId;
    return freeBlock->pid;
}

SharedMemoryData::Process MockSharedMemoryInterface::sharedMemoryBlock(
    SharedMemoryDataPtr memory,
    PidType processPid)
{
    auto currentBlock = memory->findProcess(processPid);
    return currentBlock ? *currentBlock : SharedMemoryData::Process{};
}

void MockSharedMemoryInterface::setSharedMemoryBlock(
    SharedMemoryDataPtr memory,
    SharedMemoryData::Process data,
    PidType processPid)
{
    data.pid = processPid;
    auto currentBlock = addProcessIfNeeded(memory, processPid);
    NX_ASSERT(currentBlock != nullptr);
    *currentBlock = data;
}

} // namespace test
} // namespace nx::vms::client::desktop
