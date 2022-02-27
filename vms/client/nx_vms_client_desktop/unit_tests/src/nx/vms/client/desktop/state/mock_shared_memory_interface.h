// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <memory>

#include <nx/vms/client/desktop/state/shared_memory_manager.h>

#include <nx/utils/log/assert.h>

#include "mock_client_process_execution_interface.h"

namespace nx::vms::client::desktop {
namespace test {

using SharedMemoryDataPtr = std::shared_ptr<SharedMemoryData>;

static inline SharedMemoryDataPtr makeMockSharedMemory()
{
    return std::make_shared<SharedMemoryData>();
}

class MockSharedMemoryInterface: public SharedMemoryInterface
{
public:
    MockSharedMemoryInterface(SharedMemoryDataPtr memoryMap):
        m_memory(memoryMap)
    {
    }

    virtual bool initialize() override
    {
        return true;
    }

    virtual void lock() override
    {
        NX_ASSERT(!m_locked);
        m_locked = true;
    }

    virtual void unlock() override
    {
        NX_ASSERT(m_locked);
        m_locked = false;
    }

    virtual SharedMemoryData data() const override
    {
        NX_ASSERT(m_locked);
        return *m_memory;
    }

    virtual void setData(const SharedMemoryData& data) override
    {
        NX_ASSERT(m_locked);
        *m_memory = data;
    }

    static PidType givenAnotherRunningInstance(
        SharedMemoryDataPtr memory,
        RunningProcessesMap processesMap,
        const SessionId& sessionId,
        const SharedMemoryData::ScreenUsageData& screens = {});

    static SharedMemoryData::Process sharedMemoryBlock(
        SharedMemoryDataPtr memory,
        PidType processPid);

    static void setSharedMemoryBlock(
        SharedMemoryDataPtr memory,
        SharedMemoryData::Process data,
        PidType processPid);

private:
    bool m_locked = false;
    SharedMemoryDataPtr const m_memory;
};

inline std::unique_ptr<SharedMemoryManager> makeMockSharedMemoryManager(
    SharedMemoryDataPtr memory,
    RunningProcessesMap processesMap)
{
    return std::make_unique<SharedMemoryManager>(
        std::make_unique<MockSharedMemoryInterface>(memory),
        std::make_unique<MockClientProcessExecutionInterface>(processesMap));
}

} // namespace test
} // namespace nx::vms::client::desktop
