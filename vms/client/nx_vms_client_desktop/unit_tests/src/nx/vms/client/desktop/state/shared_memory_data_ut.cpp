// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <algorithm>

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/state/shared_memory_data.h>

namespace nx::vms::client::desktop {
namespace test {
namespace {

const auto kLocalSystemId = nx::Uuid::createUuid();

const SessionId sessionId1{kLocalSystemId, "systemId", "user1"};
const SessionId sessionId2{kLocalSystemId, "systemId", "user2"};
const SessionId sessionId3{kLocalSystemId, "systemId", "user3"};

using Process = SharedMemoryData::Process;
using Session = SharedMemoryData::Session;

} // namespace

using namespace shared_memory;

TEST(SharedMemoryData, addProcess)
{
    SharedMemoryData data;
    data.addProcess(1);
    data.addProcess(2);
    data.addProcess(3);

    const SharedMemoryData::Processes expected = {{
        {.pid = 1},
        {.pid = 2},
        {.pid = 3},
    }};

    ASSERT_EQ(data.processes, expected);
}

TEST(SharedMemoryData, findProcess)
{
    SharedMemoryData data;
    data.addProcess(1);
    data.addProcess(2);
    ASSERT_TRUE(data.findProcess(1)->pid == 1);
    ASSERT_TRUE(data.findProcess(3) == nullptr);
}

TEST(SharedMemoryData, addSession)
{
    SharedMemoryData data;
    data.addSession(sessionId1);
    data.addSession(sessionId2);
    data.addSession(sessionId3);

    const SharedMemoryData::Sessions expected = {{
        {.id = sessionId1},
        {.id = sessionId2},
        {.id = sessionId3},
    }};

    ASSERT_EQ(data.sessions, expected);
}

TEST(SharedMemoryData, findSession)
{
    SharedMemoryData data;
    data.addSession(sessionId1);
    data.addSession(sessionId2);
    ASSERT_TRUE(data.findSession(sessionId1)->id == sessionId1);
    ASSERT_TRUE(data.findSession(sessionId3) == nullptr);
}

TEST(SharedMemoryData, findProcessSession)
{
    SharedMemoryData data;
    data.addProcess(1);
    data.addProcess(2);
    data.addSession(sessionId1);
    data.addSession(sessionId2);
    data.findProcess(2)->sessionId = sessionId2;

    ASSERT_TRUE(data.findProcessSession(1) == nullptr);
    ASSERT_TRUE(data.findProcessSession(2)->id == sessionId2);
}

TEST(SharedMemoryDataFilters, exceptPids)
{
    const SharedMemoryData::Processes processes = {{
        {.pid = 1},
        {.pid = 2},
        {.pid = 3},
        {.pid = 4},
    }};

    QVector<Process> result;
    std::ranges::copy(processes | exceptPids({0, 1, 3}), std::back_inserter(result));

    const QVector<Process> expected = {
        processes[1],
        processes[3],
    };

    ASSERT_EQ(result, expected);
}

TEST(SharedMemoryDataFilters, withinSession)
{
    const SharedMemoryData::Processes processes = {{
        {.pid = 1},
        {.pid = 2, .sessionId = sessionId2},
        {.pid = 3},
        {.pid = 4, .sessionId = sessionId2},
    }};

    QVector<Process> result;
    std::ranges::copy(processes | withinSession(sessionId2), std::back_inserter(result));

    const QVector<Process> expected = {
        processes[1],
        processes[3],
    };

    ASSERT_EQ(result, expected);
}

TEST(SharedMemoryDataFilters, all)
{
    const SharedMemoryData::Processes processes = {{
        {.pid = 1},
        {.pid = 2, .sessionId = sessionId2},
        {.pid = 3},
        {.pid = 4, .sessionId = sessionId2},
    }};

    SharedMemoryData::Processes result;
    std::ranges::copy(processes | all(), result.begin());

    ASSERT_EQ(result, processes);
}

} // namespace test
} // namespace nx::vms::client::desktop
