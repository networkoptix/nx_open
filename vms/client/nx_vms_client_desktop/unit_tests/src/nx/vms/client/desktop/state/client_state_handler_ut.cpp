// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <array>
#include <memory>

#include <gtest/gtest.h>

#include <client/client_runtime_settings.h>
#include <client/client_startup_parameters.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/random.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/uuid.h>
#include <nx/vms/client/desktop/state/client_state_handler.h>
#include <nx/vms/client/desktop/state/client_state_storage.h>
#include <nx/vms/client/desktop/state/session_state.h>
#include <nx/vms/client/desktop/state/shared_memory_manager.h>
#include <nx/vms/client/desktop/state/startup_parameters.h>
#include <nx/vms/client/desktop/state/window_geometry_manager.h>
#include <nx/vms/client/desktop/state/window_geometry_state.h>

#include "mock_client_process_execution_interface.h"
#include "mock_shared_memory_interface.h"
#include "mock_window_control_interface.h"

namespace nx::vms::client::desktop {
namespace test {

namespace {

static const StartupParameters kEmptyStartupParameters{};
static const QString kExampleSystemId = QnUuid::createUuid().toSimpleString();
static const QString kExampleUserName = "SomeUser@someSystem.com";
static const SessionId kSessionId(kExampleSystemId, kExampleUserName);
static const QString kDelegateId = "unit_test_" + QnUuid::createUuid().toSimpleString();
static const int kNumberOfDelegates = 2;
static const QString kRandomFilenameBase = QnUuid::createUuid().toSimpleString();
static const core::LogonData kLogonData;

/**
 * Some data structure, representing arbitrary stateful element.
 * Contains both system-independent and system-specific parameters,
 * which may be uncommon but is useful for testing purposes.
 */
struct MockStatefulElement
{
    QnUuid independent;
    QnUuid specific;

    bool operator==(const MockStatefulElement& other) const
    {
        return independent == other.independent && specific == other.specific;
    }

    bool operator!=(const MockStatefulElement& other) const
    {
        return !(*this == other);
    }
};
const QString kIndependentKey = "independent";
const QString kSpecificKey = "specific";

/**
 * Data structure that aggregates several stateful elements.
 * Also provides comparison operators for ASSERT_* macroses.
 */
struct Session
{
    std::array<MockStatefulElement, kNumberOfDelegates> data;

    bool operator==(const Session& other) const
    {
        for (int i = 0; i < kNumberOfDelegates; ++i)
        {
            if (data[i] != other.data[i])
                return false;
        }

        return true;
    }

    bool operator!=(const Session& other) const
    {
        return !(*this == other);
    }
};

using SessionSubstate = std::array<QnUuid, kNumberOfDelegates>;

class MockStatefulDelegate: public ClientStateDelegate
{
public:
    MockStatefulDelegate(Session& session, int index):
        m_session(session),
        m_index(index)
    {
    }

    virtual bool loadState(
        const DelegateState& state,
        SubstateFlags flags,
        const StartupParameters& params) override
    {
        if (flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
            data().independent = QnUuid(state.value(kIndependentKey).toString());

        if (flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            data().specific = QnUuid(state.value(kSpecificKey).toString());

        return true;
    }

    virtual void saveState(DelegateState* state, SubstateFlags flags) override
    {
        if (flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
            state->insert(kIndependentKey, data().independent.toString());

        if (flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            state->insert(kSpecificKey, data().specific.toString());
    }

private:
    MockStatefulElement& data() const
    {
        return m_session.data[m_index];
    }

private:
    Session& m_session;
    int m_index;
};

Session randomData()
{
    Session session;
    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        session.data[i].independent = QnUuid::createUuid();
        session.data[i].specific = QnUuid::createUuid();
    }
    return session;
}

/** Converts Session struct into a map representation used by ClientStateStorage. */
SessionState makeSessionState(const Session& session, ClientStateDelegate::SubstateFlags flags)
{
    SessionState result;

    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        DelegateState substate;

        if (flags.testFlag(ClientStateDelegate::Substate::systemIndependentParameters))
            substate[kIndependentKey] = session.data[i].independent.toString();

        if (flags.testFlag(ClientStateDelegate::Substate::systemSpecificParameters))
            substate[kSpecificKey] = session.data[i].specific.toString();

        result[QString("%1_%2").arg(kDelegateId).arg(i)] = substate;
    }

    return result;
}

} // namespace

class ClientStateHandlerTest:
    public testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        m_clientRuntimeSettings = std::make_unique<QnClientRuntimeSettings>(QnStartupParameters{});
        m_sharedMemory = makeMockSharedMemory();
        m_processesMap = makeMockProcessesMap();

        m_clientStateHandler = std::make_unique<ClientStateHandler>(testDataDir());

        m_sharedMemoryManager = makeMockSharedMemoryManager(m_sharedMemory, m_processesMap);
        m_clientStateHandler->setSharedMemoryManager(m_sharedMemoryManager.get());

        m_clientStateHandler->setClientProcessExecutionInterface(
            std::make_unique<MockClientProcessExecutionInterface>(m_processesMap));

        for (int i = 0; i < kNumberOfDelegates; ++i)
        {
            auto stateDelegate = std::make_unique<MockStatefulDelegate>(m_session, i);
            m_clientStateHandler->registerDelegate(QString("%1_%2").arg(kDelegateId).arg(i), std::move(stateDelegate));
        }

        randomizeCurrentSession();
        m_sharedMemoryManager->enterSession(kSessionId);
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        if (m_commonState)
            QFile::remove(ClientStateStorage::kCommonSubstateFileName);
        if (m_systemState)
            QFile::remove("kSessionId/" + ClientStateStorage::kSystemSubstateFileName);
        if (m_temporaryState)
            QFile::remove(m_temporaryState->path);
        for (const auto session: m_savedSessions)
            QFile::remove(session.path);

        m_sharedMemoryManager.reset();
        m_clientStateHandler.reset();
        m_processesMap.reset();
        m_sharedMemory.reset();

        m_commonState = std::nullopt;
        m_systemState = std::nullopt;
        m_temporaryState = std::nullopt;
        m_savedSessions.clear();
        m_clientRuntimeSettings.reset();
    }

protected:
    struct SavedSession: public Session
    {
        SavedSession(Session&& data): Session(data) {}
        SavedSession() = default; //< Required by QMap.
        QString filename;
        QString path;
    };

    ClientStateHandler* clientStateHandler() const
    {
        return m_clientStateHandler.get();
    }

    QString createFile(
        const QString& filename,
        const QByteArray& data,
        const QString& subdirectory = QString())
    {
        QDir directory(testDataDir());
        if (!subdirectory.isEmpty())
        {
            NX_ASSERT(directory.mkpath(directory.absolutePath() + "/" + subdirectory));
            NX_ASSERT(directory.cd(subdirectory));
        }

        const QString filePath = directory.absoluteFilePath(filename);
        QFile file(filePath);
        NX_ASSERT(file.open(QIODevice::WriteOnly));
        file.write(data);
        file.close();

        return filePath;
    }

    void givenCommonSubstate()
    {
        if (!NX_ASSERT(!m_commonState.has_value()))
            return;

        m_commonState = std::array<QnUuid, kNumberOfDelegates>();

        Session temp;
        for (int i = 0; i < kNumberOfDelegates; ++i)
        {
            temp.data[i].independent = (*m_commonState)[i] = QnUuid::createUuid();
        }

        createFile(
            ClientStateStorage::kCommonSubstateFileName,
            QJson::serialized(makeSessionState(
                temp,
                ClientStateDelegate::Substate::systemIndependentParameters)));
    }

    void givenSystemSubstate()
    {
        if (!NX_ASSERT(!m_systemState.has_value()))
            return;

        m_systemState = std::array<QnUuid, kNumberOfDelegates>();

        Session temp;
        for (int i = 0; i < kNumberOfDelegates; ++i)
        {
            temp.data[i].independent = QnUuid::createUuid();
            temp.data[i].specific = (*m_systemState)[i] = QnUuid::createUuid();
        }

        // Store all parameters just to check that system-independent parameters are ignored on loading.
        createFile(
            ClientStateStorage::kSystemSubstateFileName,
            QJson::serialized(makeSessionState(temp, ClientStateDelegate::Substate::allParameters)),
            kSessionId);
    }

    void givenTemporaryState()
    {
        if (!NX_ASSERT(!m_temporaryState.has_value()))
            return;

        SavedSession session(randomData());
        session.filename = kRandomFilenameBase + ".json";

        const SessionState sessionState = makeSessionState(
            session,
            ClientStateDelegate::Substate::allParameters);
        session.path = createFile(
            session.filename,
            QJson::serialized(sessionState),
            ClientStateStorage::kTemporaryStatesDirName);

        m_temporaryState = session;
    }

    void givenSavedSession(int id = 0)
    {
        SavedSession session(randomData());
        session.filename = kRandomFilenameBase + QString::number(id) + ".json";

        const SessionState sessionState = makeSessionState(
            session,
            ClientStateDelegate::Substate::allParameters);
        session.path = createFile(
            session.filename,
            QJson::serialized(sessionState),
            kSessionId);

        m_savedSessions.insert(id, session);
    }

    PidType givenAnotherRunningInstance(const SessionId& sessionId)
    {
        return MockSharedMemoryInterface::givenAnotherRunningInstance(
            m_sharedMemory,
            m_processesMap,
            sessionId);
    }

    int runningProcessesCount()
    {
        MockClientProcessExecutionInterface processesInterface(m_processesMap);
        const RunningProcessesMapType runningProcesses = processesInterface.runningProcesses();
        return runningProcesses.size();
    }

    int runningProcessesCountBySession(const SessionId& sessionId)
    {
        MockClientProcessExecutionInterface processesInterface(m_processesMap);
        const RunningProcessesMapType runningProcesses = processesInterface.runningProcesses();
        return std::count_if(runningProcesses.cbegin(), runningProcesses.cend(),
            [&sessionId](auto iter)
            {
                return iter.sessionId == sessionId;
            });
    }

    SharedMemoryData::Process sharedMemoryBlock(PidType processId)
    {
        return MockSharedMemoryInterface::sharedMemoryBlock(m_sharedMemory, processId);
    }

    SharedMemoryData::Process currentSharedMemoryBlock()
    {
        return sharedMemoryBlock(MockClientProcessExecutionInterface::kCurrentProcessPid);
    }

    void randomizeCurrentSession()
    {
        m_session = randomData();
    }

    SessionSubstate commonSubstate() const
    {
        if (NX_ASSERT(m_commonState))
            return *m_commonState;
        return {};
    }

    SessionSubstate systemSubstate() const
    {
        if (NX_ASSERT(m_systemState))
            return *m_systemState;
        return {};
    }

    SavedSession temporaryState() const
    {
        if (NX_ASSERT(m_temporaryState))
            return *m_temporaryState;
        return {};
    }

    SavedSession savedSession(int id = 0)
    {
        NX_ASSERT(m_savedSessions.contains(id));
        return m_savedSessions.value(id);
    }

    Session currentSession()
    {
        return m_session;
    }

    QnClientRuntimeSettings* clientRuntimeSettings()
    {
        return m_clientRuntimeSettings.get();
    }

private:
    std::unique_ptr<QnClientRuntimeSettings> m_clientRuntimeSettings;
    std::unique_ptr<SharedMemoryManager> m_sharedMemoryManager;
    std::unique_ptr<ClientStateHandler> m_clientStateHandler;
    std::unique_ptr<MockClientProcessExecutionInterface> m_clientProcessExecutionInterface;
    SharedMemoryDataPtr m_sharedMemory;
    RunningProcessesMap m_processesMap;

    std::optional<SessionSubstate> m_commonState;
    std::optional<SessionSubstate> m_systemState;
    std::optional<SavedSession> m_temporaryState;
    QMap<int, SavedSession> m_savedSessions;

    Session m_session;
};

TEST_F(ClientStateHandlerTest, defaultStateOnStartup)
{
    randomizeCurrentSession();

    clientStateHandler()->clientStarted(kEmptyStartupParameters);
    for (const auto& state: currentSession().data)
    {
        // State should be set to default, so e.g. window geometry will be correct on clean startup.
        ASSERT_EQ(state.independent, QnUuid());
    }
}

TEST_F(ClientStateHandlerTest, loadCommonStateOnStartup)
{
    givenCommonSubstate();
    clientStateHandler()->clientStarted(kEmptyStartupParameters);
    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        ASSERT_EQ(currentSession().data[i].independent, commonSubstate()[i]);
    }
}

TEST_F(ClientStateHandlerTest, saveCommonStateOnClosing)
{
    const auto lastSession = currentSession();
    clientStateHandler()->storeSystemIndependentState();
    randomizeCurrentSession();

    clientStateHandler()->clientStarted(kEmptyStartupParameters);
    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        ASSERT_EQ(currentSession().data[i].independent, lastSession.data[i].independent);
    }
}

TEST_F(ClientStateHandlerTest, commonStateIsNotSavedOnClosingInVideowallMode)
{
    clientRuntimeSettings()->setVideoWallMode(true);
    const auto lastSession = currentSession();
    clientStateHandler()->storeSystemIndependentState();
    randomizeCurrentSession();

    clientStateHandler()->clientStarted(kEmptyStartupParameters);
    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        ASSERT_NE(currentSession().data[i].independent, lastSession.data[i].independent);
    }
}

TEST_F(ClientStateHandlerTest, DISABLED_noSystemSubstate)
{
    randomizeCurrentSession();
    const auto lastSession = currentSession();

    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);

    // Data were not changed.
    ASSERT_EQ(currentSession(), lastSession);
}

TEST_F(ClientStateHandlerTest, loadSystemSubstate)
{
    givenCommonSubstate();
    givenSystemSubstate();
    randomizeCurrentSession();

    clientStateHandler()->clientStarted(kEmptyStartupParameters);
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);

    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        ASSERT_EQ(currentSession().data[i].independent, commonSubstate()[i]);
        ASSERT_EQ(currentSession().data[i].specific, systemSubstate()[i]);
    }
}

TEST_F(ClientStateHandlerTest, saveSystemSubstateOnDisconnect)
{
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);
    const auto lastSession = currentSession();
    clientStateHandler()->clientDisconnected();
    randomizeCurrentSession();

    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);
    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        ASSERT_EQ(currentSession().data[i].specific, lastSession.data[i].specific);
    }
}

TEST_F(ClientStateHandlerTest, systemSubstateIsNotSavedOnDisconnectInVideoWallMode)
{
    clientRuntimeSettings()->setVideoWallMode(true);
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);
    const auto lastSession = currentSession();
    clientStateHandler()->clientDisconnected();
    randomizeCurrentSession();

    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);
    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        ASSERT_NE(currentSession().data[i].specific, lastSession.data[i].specific);
    }
}

TEST_F(ClientStateHandlerTest, loadTemporaryState)
{
    givenTemporaryState();
    randomizeCurrentSession();

    StartupParameters startupParameters
    {
        StartupParameters::Mode::inheritState,
        kSessionId,
        temporaryState().filename
    };

    clientStateHandler()->clientStarted(startupParameters);
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);

    ASSERT_EQ(currentSession(), temporaryState());
    ASSERT_FALSE(QFile::exists(temporaryState().path));
}

TEST_F(ClientStateHandlerTest, createNewWindow)
{
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);

    clientStateHandler()->createNewWindow();
    clientStateHandler()->createNewWindow(kLogonData);
    clientStateHandler()->createNewWindow(kLogonData, {""});
    ASSERT_EQ(runningProcessesCount(), 4);
    ASSERT_EQ(runningProcessesCountBySession(kSessionId), 2); //< The current one doesn't count.
}

TEST_F(ClientStateHandlerTest, fullRestoreOnConnect)
{
    // Assuming states are sorted by the alphabet.
    givenSavedSession(0);
    givenSavedSession(1);
    randomizeCurrentSession();

    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ true, kSessionId, kLogonData);

    ASSERT_EQ(currentSession(), savedSession(0));

    ASSERT_EQ(runningProcessesCount(), 2);
    ASSERT_EQ(runningProcessesCountBySession(kSessionId), 1); //< The current one doesn't count.
}

TEST_F(ClientStateHandlerTest, skipFullRestoreWhenStateIsNotSaved)
{
    givenSystemSubstate();

    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ true, kSessionId, kLogonData);

    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        ASSERT_EQ(currentSession().data[i].specific, systemSubstate()[i]);
    }
}

TEST_F(ClientStateHandlerTest, skipFullRestoreWhenItIsDisabled)
{
    givenSystemSubstate();
    givenSavedSession(0);
    givenSavedSession(1);

    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);

    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        ASSERT_EQ(currentSession().data[i].specific, systemSubstate()[i]);
    }
    ASSERT_EQ(runningProcessesCount(), 1);
}

TEST_F(ClientStateHandlerTest, skipFullRestoreWhenSessionIsStartedAlready)
{
    givenSystemSubstate();
    givenSavedSession();

    givenAnotherRunningInstance(kSessionId);
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ true, kSessionId, kLogonData);

    for (int i = 0; i < kNumberOfDelegates; ++i)
    {
        ASSERT_EQ(currentSession().data[i].specific, systemSubstate()[i]);
    }
}

TEST_F(ClientStateHandlerTest, saveWindowsConfiguration)
{
    givenAnotherRunningInstance(kSessionId);
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ true, kSessionId, kLogonData);

    clientStateHandler()->saveWindowsConfiguration();

    ASSERT_TRUE(clientStateHandler()->hasSavedConfiguration()); //< TODO: Count states.
}

TEST_F(ClientStateHandlerTest, restoreWindowsConfiguration1)
{
    givenSavedSession(0);
    givenSavedSession(1);
    const auto firstId = givenAnotherRunningInstance(kSessionId);
    const auto secondId = givenAnotherRunningInstance(kSessionId);

    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);

    clientStateHandler()->restoreWindowsConfiguration(kLogonData);

    ASSERT_EQ(currentSession(), savedSession(0));
    ASSERT_EQ(runningProcessesCount(), 3);
    ASSERT_EQ(sharedMemoryBlock(firstId).command, SharedMemoryData::Command::restoreWindowState);
    ASSERT_EQ(sharedMemoryBlock(secondId).command, SharedMemoryData::Command::none);
}

TEST_F(ClientStateHandlerTest, restoreWindowsConfiguration2)
{
    givenSavedSession(0);
    givenSavedSession(1);

    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ false, kSessionId, kLogonData);

    clientStateHandler()->restoreWindowsConfiguration(kLogonData);

    ASSERT_EQ(currentSession(), savedSession(0));
    ASSERT_EQ(runningProcessesCount(), 2);
    // TODO: check params of the created process.
}

TEST_F(ClientStateHandlerTest, deleteWindowsConfiguration)
{
    givenSavedSession();
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ true, kSessionId, kLogonData);

    ASSERT_TRUE(clientStateHandler()->hasSavedConfiguration());

    clientStateHandler()->deleteWindowsConfiguration();
    ASSERT_FALSE(clientStateHandler()->hasSavedConfiguration());
}

TEST_F(ClientStateHandlerTest, requestedToSaveState)
{
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ true, kSessionId, kLogonData);

    ASSERT_FALSE(clientStateHandler()->hasSavedConfiguration());

    clientStateHandler()->clientRequestedToSaveState();
    ASSERT_TRUE(clientStateHandler()->hasSavedConfiguration());
}

TEST_F(ClientStateHandlerTest, requestToLoadState)
{
    givenSavedSession();
    clientStateHandler()->connectionToSystemEstablished(/*fullRestoreEnabled*/ true, kSessionId, kLogonData);

    clientStateHandler()->clientRequestedToRestoreState(savedSession(0).filename);
    ASSERT_EQ(currentSession(), savedSession(0));
}

} // namespace test
} // namespace nx::vms::client::desktop
