// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/state/client_state_storage.h>
#include <nx/vms/client/desktop/state/session_id.h>
#include <nx/vms/client/desktop/state/window_geometry_state.h>

#include <nx/fusion/model_functions.h>

#include <nx/utils/random.h>
#include <nx/utils/test_support/test_with_temporary_directory.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

static const QString kExampleSystemId = QnUuid::createUuid().toString();
static const QString kExampleUserName = "SomeUser@someSystem.com";
static const SessionId kSessionId(kExampleSystemId, kExampleUserName);

SessionState makeRandomSessionState()
{
    SessionState result;
    result["dummy_delegate"]["val1"] = nx::utils::random::number(0, 1000);
    result["dummy_delegate"]["val2"] = nx::utils::random::choice({ "s1", "s2", "s3" });
    result["another_delegate"]["v0"] = QJsonObject({ {"a", 1}, {"b", "c"} });
    return result;
}

} // namespace

class ClientStateStorageTest:
    public testing::Test,
    public nx::utils::test::TestWithTemporaryDirectory
{
public:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        m_clientStateStorage = std::make_unique<ClientStateStorage>(testDataDir());
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        m_clientStateStorage.reset();
    }

    ClientStateStorage* stateStorage() const
    {
        return m_clientStateStorage.get();
    }

    void createFile(
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

        QFile file(directory.absoluteFilePath(filename));
        file.open(QIODevice::WriteOnly);
        file.write(data);
        file.close();
    }

private:
    std::unique_ptr<ClientStateStorage> m_clientStateStorage;
};

TEST_F(ClientStateStorageTest, readCommonSubstate)
{
    const auto data = makeRandomSessionState();
    createFile(
        ClientStateStorage::kCommonSubstateFileName,
        QJson::serialized(data));
    auto state = stateStorage()->readCommonSubstate();
    ASSERT_EQ(state, data);
}

TEST_F(ClientStateStorageTest, writeCommonSubstate)
{
    const auto data = makeRandomSessionState();
    stateStorage()->writeCommonSubstate(data);
    auto state = stateStorage()->readCommonSubstate();
    ASSERT_EQ(state, data);
}

TEST_F(ClientStateStorageTest, readSystemSubstate)
{
    const auto data = makeRandomSessionState();
    createFile(
        ClientStateStorage::kSystemSubstateFileName,
        QJson::serialized(data),
        kSessionId);
    auto state = stateStorage()->readSystemSubstate(kSessionId);
    ASSERT_EQ(state, data);
}

TEST_F(ClientStateStorageTest, writeSystemSubstate)
{
    const auto data = makeRandomSessionState();
    stateStorage()->writeSystemSubstate(kSessionId, data);
    auto state = stateStorage()->readSystemSubstate(kSessionId);
    ASSERT_EQ(state, data);
}

TEST_F(ClientStateStorageTest, takeTemporaryState)
{
    const auto data = makeRandomSessionState();
    createFile(
        "t1",
        QJson::serialized(data),
        ClientStateStorage::kTemporaryStatesDirName);
    auto state = stateStorage()->takeTemporaryState("t1");
    ASSERT_EQ(state, data);
}

TEST_F(ClientStateStorageTest, putTemporaryState)
{
    const auto data = makeRandomSessionState();
    stateStorage()->putTemporaryState("t1", data);
    auto state = stateStorage()->takeTemporaryState("t1");
    ASSERT_EQ(state, data);
}

TEST_F(ClientStateStorageTest, cleanSessionState)
{
    const auto data = makeRandomSessionState();
    stateStorage()->writeSystemSubstate(kSessionId, data); //< Should not be deleted.

    createFile("1", QJson::serialized(makeRandomSessionState()), kSessionId);
    createFile("2", QJson::serialized(makeRandomSessionState()), kSessionId);
    createFile("3", QJson::serialized(makeRandomSessionState()), kSessionId);

    stateStorage()->clearSession(kSessionId);
    ASSERT_EQ(stateStorage()->readFullSessionState(kSessionId).size(), 0);
    ASSERT_EQ(stateStorage()->readSystemSubstate(kSessionId), data);
}

TEST_F(ClientStateStorageTest, readSessionState)
{
    stateStorage()->clearSession(kSessionId);

    auto state1 = makeRandomSessionState();
    auto state2 = makeRandomSessionState();
    auto state3 = makeRandomSessionState();
    createFile("1", QJson::serialized(state1), kSessionId);
    createFile("2", QJson::serialized(state2), kSessionId);
    createFile("3", QJson::serialized(state3), kSessionId);

    auto fullState = stateStorage()->readFullSessionState(kSessionId);
    ASSERT_EQ(fullState.size(), 3);
    ASSERT_EQ(fullState["1"], state1);
    ASSERT_EQ(fullState["2"], state2);
    ASSERT_EQ(fullState["3"], state3);

    ASSERT_EQ(stateStorage()->readSessionState(kSessionId, "1"), state1);
    ASSERT_EQ(stateStorage()->readSessionState(kSessionId, "2"), state2);
    ASSERT_EQ(stateStorage()->readSessionState(kSessionId, "3"), state3);
}

TEST_F(ClientStateStorageTest, writeSessionState)
{
    stateStorage()->clearSession(kSessionId);

    auto state1 = makeRandomSessionState();
    auto state2 = makeRandomSessionState();
    auto state3 = makeRandomSessionState();

    stateStorage()->writeSessionState(kSessionId, "1", state1);
    stateStorage()->writeSessionState(kSessionId, "2", state2);
    stateStorage()->writeSessionState(kSessionId, "3", state3);

    auto fullState = stateStorage()->readFullSessionState(kSessionId);
    ASSERT_EQ(fullState.size(), 3);
    ASSERT_EQ(fullState["1"], state1);
    ASSERT_EQ(fullState["2"], state2);
    ASSERT_EQ(fullState["3"], state3);
}

} // namespace test
} // namespace nx::vms::client::desktop
