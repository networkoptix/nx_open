// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <deque>
#include <memory>

#include <QtCore/QCoreApplication>

#include <nx/vms/client/desktop/state/shared_memory_manager.h>

#include <nx/utils/random.h>
#include <nx/utils/uuid.h>

#include "mock_client_process_execution_interface.h"
#include "mock_shared_memory_interface.h"

namespace nx::vms::client::desktop {
namespace test {

using Command = SharedMemoryData::Command;

namespace {

static const SessionId kSessionId("some_system_id", "some_user_name");
static const SessionId kAnotherSessionId("some_other_system_id", "some_other_user_name");

class CommandQueue: public QObject
{
public:
    void commandReceived(Command command)
    {
        m_queue.push_back(command);
    }

    Command pop_front()
    {
        NX_ASSERT(!m_queue.empty());
        Command result = m_queue.front();
        m_queue.pop_front();
        return result;
    }

    bool empty() const
    {
        return m_queue.empty();
    }

private:
    std::deque<Command> m_queue;
};

} // namespace

class SharedMemoryManagerTest: public testing::Test
{
public:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        m_sharedMemory = makeMockSharedMemory();
        m_processesMap = makeMockProcessesMap();
        m_sharedMemoryManager = makeMockSharedMemoryManager(m_sharedMemory, m_processesMap);

        m_commandQueue = std::make_unique<CommandQueue>();
        QObject::connect(m_sharedMemoryManager.get(), &SharedMemoryManager::clientCommandRequested,
            m_commandQueue.get(), &CommandQueue::commandReceived);
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        ASSERT_TRUE(m_commandQueue->empty());
        m_commandQueue.reset();
        m_sharedMemoryManager.reset();
        m_sharedMemory.reset();
        m_processesMap.reset();
    }

protected:
    SharedMemoryManager* sharedMemoryManager() const
    {
        return m_sharedMemoryManager.get();
    }

    void givenAnotherRunningInstance(
        const SessionId& sessionId,
        const SharedMemoryData::ScreenUsageData& screens = {})
    {
        MockSharedMemoryInterface::givenAnotherRunningInstance(
            m_sharedMemory,
            m_processesMap,
            sessionId,
            screens);
    }

    void whenCommandIsSendByAnotherInstance(Command command)
    {
        auto data = currentSharedMemoryProcess();
        NX_ASSERT(data.command == Command::none);
        data.command = command;
        MockSharedMemoryInterface::setSharedMemoryBlock(
            m_sharedMemory,
            data,
            MockClientProcessExecutionInterface::kCurrentProcessPid);
    }

    SharedMemoryData::Process currentSharedMemoryProcess() const
    {
        return MockSharedMemoryInterface::sharedMemoryBlock(
            m_sharedMemory,
            MockClientProcessExecutionInterface::kCurrentProcessPid);
    }

    SharedMemoryData::Process rawProcessSharedMemoryAt(int index)
    {
        NX_ASSERT(index >= 0 && index < m_sharedMemory->processes.size());
        return m_sharedMemory->processes.at(index);
    }

    Command actualCommand()
    {
        return m_commandQueue->pop_front();
    }

private:
    std::unique_ptr<SharedMemoryManager> m_sharedMemoryManager;
    SharedMemoryDataPtr m_sharedMemory;
    RunningProcessesMap m_processesMap;
    std::unique_ptr<CommandQueue> m_commandQueue;
};

TEST(SharedMemoryManager, deadInstancesAreCleanedUpOnStart)
{
    auto sharedMemory = makeMockSharedMemory();
    auto processesMap = makeMockProcessesMap();
    auto pid = MockSharedMemoryInterface::givenAnotherRunningInstance(
        sharedMemory,
        processesMap,
        kSessionId);

    MockClientProcessExecutionInterface processInterface(processesMap);
    processInterface.whenProcessIsKilled(pid);

    // Manager must cleanup dead session on startup.
    auto manager = makeMockSharedMemoryManager(sharedMemory, processesMap);
    ASSERT_FALSE(manager->sessionIsRunningAlready(kSessionId));
}

TEST(SharedMemoryManager, currentInstanceIsUnregisteredOnClose)
{
    const auto currentPid = MockClientProcessExecutionInterface::kCurrentProcessPid;
    auto sharedMemory = makeMockSharedMemory();
    auto processesMap = makeMockProcessesMap();
    auto manager = makeMockSharedMemoryManager(sharedMemory, processesMap);

    ASSERT_EQ(MockSharedMemoryInterface::sharedMemoryBlock(sharedMemory, currentPid).pid, currentPid);

    // Manager must cleanup his own session on closing.
    manager.reset();
    ASSERT_EQ(MockSharedMemoryInterface::sharedMemoryBlock(sharedMemory, currentPid).pid, 0);
}

TEST_F(SharedMemoryManagerTest, currentProcessIsRegisteredAtStart)
{
    ASSERT_TRUE(currentSharedMemoryProcess().pid != 0);
}

TEST_F(SharedMemoryManagerTest, sessionIsRunningAlready)
{
    ASSERT_FALSE(sharedMemoryManager()->sessionIsRunningAlready(kSessionId));
    givenAnotherRunningInstance(kSessionId);
    ASSERT_TRUE(sharedMemoryManager()->sessionIsRunningAlready(kSessionId));
}

TEST_F(SharedMemoryManagerTest, leaveSession)
{
    sharedMemoryManager()->enterSession(kSessionId);
    ASSERT_FALSE(sharedMemoryManager()->leaveSession()) << "Session is still active";
}

TEST_F(SharedMemoryManagerTest, leaveActiveSession)
{
    givenAnotherRunningInstance(kSessionId);
    sharedMemoryManager()->enterSession(kSessionId);
    ASSERT_TRUE(sharedMemoryManager()->leaveSession()) << "Session is not active";
}

TEST_F(SharedMemoryManagerTest, currentInstanceIndexWhenRunFirst)
{
    ASSERT_EQ(sharedMemoryManager()->currentInstanceIndex(), 0);
}

TEST(SharedMemoryManager, currentInstanceIndexWhenRunSecond)
{
    auto sharedMemory = makeMockSharedMemory();
    auto processesMap = makeMockProcessesMap();
    auto pid = MockSharedMemoryInterface::givenAnotherRunningInstance(
        sharedMemory,
        processesMap,
        kSessionId);
    auto manager = makeMockSharedMemoryManager(sharedMemory, processesMap);
    ASSERT_EQ(manager->currentInstanceIndex(), 1);
}

TEST_F(SharedMemoryManagerTest, checkRunningInstancesIndices)
{
    ASSERT_EQ(sharedMemoryManager()->runningInstancesIndices().size(), 1);
    ASSERT_EQ(sharedMemoryManager()->runningInstancesIndices()[0], 0);
    givenAnotherRunningInstance(kSessionId);
    ASSERT_EQ(sharedMemoryManager()->runningInstancesIndices().size(), 2);
    ASSERT_EQ(sharedMemoryManager()->runningInstancesIndices()[0], 0);
    ASSERT_EQ(sharedMemoryManager()->runningInstancesIndices()[1], 1);
}

TEST_F(SharedMemoryManagerTest, checkUsedScreens)
{
    SharedMemoryData::ScreenUsageData localScreens{1, 7, 12};
    SharedMemoryData::ScreenUsageData otherScreens{6, 12, 14};
    givenAnotherRunningInstance(kSessionId, otherScreens);
    ASSERT_EQ(sharedMemoryManager()->allUsedScreens(), otherScreens);
    sharedMemoryManager()->setCurrentInstanceScreens(localScreens);
    ASSERT_EQ(sharedMemoryManager()->allUsedScreens(), localScreens + otherScreens);
}

TEST_F(SharedMemoryManagerTest, requestToSaveState)
{
    sharedMemoryManager()->enterSession(kSessionId);

    givenAnotherRunningInstance(kSessionId);
    givenAnotherRunningInstance(kAnotherSessionId);
    givenAnotherRunningInstance(SessionId());

    sharedMemoryManager()->requestToSaveState();

    ASSERT_EQ(rawProcessSharedMemoryAt(0).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(1).command, Command::saveWindowState);
    ASSERT_EQ(rawProcessSharedMemoryAt(2).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(3).command, Command::none);
}

TEST_F(SharedMemoryManagerTest, assignStatesUnderfilled)
{
    sharedMemoryManager()->enterSession(kSessionId);

    givenAnotherRunningInstance(kSessionId);
    givenAnotherRunningInstance(kSessionId);
    givenAnotherRunningInstance(kAnotherSessionId);
    givenAnotherRunningInstance(SessionId());

    auto stateName = QnUuid::createUuid().toString();
    QStringList files{ stateName };
    sharedMemoryManager()->assignStatesToOtherInstances(&files);

    // Check that all states have been assigned.
    ASSERT_EQ(files.size(), 0);

    // Check the commands.
    ASSERT_EQ(rawProcessSharedMemoryAt(0).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(1).command, Command::restoreWindowState);
    ASSERT_EQ(rawProcessSharedMemoryAt(2).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(3).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(3).command, Command::none);

    // Check the command data.
    auto receivedName = QString::fromLatin1(rawProcessSharedMemoryAt(1).commandData.constData());
    ASSERT_EQ(stateName, receivedName);
}

TEST_F(SharedMemoryManagerTest, assignStatesOverflown)
{
    sharedMemoryManager()->enterSession(kSessionId);

    givenAnotherRunningInstance(kSessionId);
    givenAnotherRunningInstance(kSessionId);
    givenAnotherRunningInstance(kAnotherSessionId);
    givenAnotherRunningInstance(SessionId());

    auto stateName1 = QnUuid::createUuid().toString();
    auto stateName2 = QnUuid::createUuid().toString();
    auto stateName3 = QnUuid::createUuid().toString();
    QStringList files{ stateName1, stateName2, stateName3 };
    sharedMemoryManager()->assignStatesToOtherInstances(&files);

    // Check unassigned states.
    ASSERT_EQ(files.size(), 1);
    ASSERT_EQ(files.first(), stateName3);

    // Check the commands.
    ASSERT_EQ(rawProcessSharedMemoryAt(0).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(1).command, Command::restoreWindowState);
    ASSERT_EQ(rawProcessSharedMemoryAt(2).command, Command::restoreWindowState);
    ASSERT_EQ(rawProcessSharedMemoryAt(3).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(3).command, Command::none);

    // Check the command data.
    auto receivedName1 = QString::fromLatin1(rawProcessSharedMemoryAt(1).commandData.constData());
    auto receivedName2 = QString::fromLatin1(rawProcessSharedMemoryAt(2).commandData.constData());
    ASSERT_EQ(stateName1, receivedName1);
    ASSERT_EQ(stateName2, receivedName2);
}

TEST_F(SharedMemoryManagerTest, requestSettingsReload)
{
    sharedMemoryManager()->enterSession(kSessionId);

    givenAnotherRunningInstance(kSessionId);
    givenAnotherRunningInstance(kAnotherSessionId);
    givenAnotherRunningInstance(SessionId());

    sharedMemoryManager()->requestSettingsReload();

    ASSERT_EQ(rawProcessSharedMemoryAt(0).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(1).command, Command::reloadSettings);
    ASSERT_EQ(rawProcessSharedMemoryAt(2).command, Command::reloadSettings);
    ASSERT_EQ(rawProcessSharedMemoryAt(3).command, Command::reloadSettings);
}

TEST_F(SharedMemoryManagerTest, sendCommandsInOrder)
{
    sharedMemoryManager()->enterSession(kSessionId);

    givenAnotherRunningInstance(kSessionId);
    givenAnotherRunningInstance(kAnotherSessionId);
    givenAnotherRunningInstance(SessionId());

    sharedMemoryManager()->requestSettingsReload();
    sharedMemoryManager()->requestToSaveState();

    ASSERT_EQ(rawProcessSharedMemoryAt(0).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(1).command, Command::saveWindowState);
    ASSERT_EQ(rawProcessSharedMemoryAt(2).command, Command::reloadSettings);
    ASSERT_EQ(rawProcessSharedMemoryAt(3).command, Command::reloadSettings);
}

TEST_F(SharedMemoryManagerTest, closeAllWindows)
{
    givenAnotherRunningInstance(kSessionId);
    givenAnotherRunningInstance(kAnotherSessionId);
    givenAnotherRunningInstance(SessionId());

    sharedMemoryManager()->requestSettingsReload();
    sharedMemoryManager()->requestToExit();

    ASSERT_EQ(rawProcessSharedMemoryAt(0).command, Command::none);
    ASSERT_EQ(rawProcessSharedMemoryAt(1).command, Command::exit);
    ASSERT_EQ(rawProcessSharedMemoryAt(2).command, Command::exit);
    ASSERT_EQ(rawProcessSharedMemoryAt(3).command, Command::exit);
}

TEST_F(SharedMemoryManagerTest, commandsProcessedInQueue)
{
    whenCommandIsSendByAnotherInstance(Command::reloadSettings);
    sharedMemoryManager()->processEvents();
    ASSERT_EQ(actualCommand(), Command::reloadSettings);
}

} // namespace test
} // namespace nx::vms::client::desktop
