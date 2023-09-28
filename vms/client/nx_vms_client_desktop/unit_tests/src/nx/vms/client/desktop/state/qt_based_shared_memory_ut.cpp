// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <gtest/gtest.h>

#include <nx/vms/client/desktop/state/qt_based_shared_memory.h>

#include <nx/utils/random.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {
namespace test {

namespace {

QString randomString()
{
    return QnUuid::createUuid().toSimpleString();
}

SharedMemoryData generateRandomData()
{
    SharedMemoryData result;
    QSet<SessionId> sessionsToAdd;
    for (auto& data: result.processes)
    {
        data.pid = nx::utils::random::number<SharedMemoryData::PidType>();
        data.sessionId = SessionId(randomString(), randomString());
        sessionsToAdd.insert(data.sessionId);
        for (int i = 0; i < 64; ++i)
        {
            if (nx::utils::random::choice({true, false}))
                data.usedScreens.insert(i);
        }
        data.command = nx::utils::random::choice({
            SharedMemoryData::Command::none,
            SharedMemoryData::Command::reloadSettings,
            SharedMemoryData::Command::exit,
            SharedMemoryData::Command::saveWindowState,
            SharedMemoryData::Command::restoreWindowState});
        if (data.command == SharedMemoryData::Command::restoreWindowState)
            data.commandData = QnUuid::createUuid().toString().toLatin1();
    }

    std::transform(sessionsToAdd.begin(), sessionsToAdd.end(), result.sessions.begin(),
        [](SessionId sessionId)
        {
            return SharedMemoryData::Session{sessionId, randomString().toStdString()};
        });

    return result;
}

} // namespace

class QtBasedSharedMemoryTest: public testing::Test
{
public:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp() override
    {
        const QString sharedMemoryId = randomString();
        m_memory1 = std::make_unique<QtBasedSharedMemory>(sharedMemoryId);
        m_memory2 = std::make_unique<QtBasedSharedMemory>(sharedMemoryId);
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown() override
    {
        m_memory1.reset();
        m_memory2.reset();
    }

protected:
    void givenMemoryInitialized()
    {
        ASSERT_TRUE(m_memory1->initialize());
        ASSERT_TRUE(m_memory2->initialize());
    }

    SharedMemoryData whenWriteRandomDataToMemory1()
    {
        SharedMemoryData data = generateRandomData();
        m_memory1->lock();
        m_memory1->setData(data);
        m_memory1->unlock();
        return data;
    }

    SharedMemoryData readDataFromMemory2()
    {
        m_memory2->lock();
        SharedMemoryData result = m_memory2->data();
        m_memory2->unlock();
        return result;
    }

private:
    std::unique_ptr<QtBasedSharedMemory> m_memory1;
    std::unique_ptr<QtBasedSharedMemory> m_memory2;
};

TEST_F(QtBasedSharedMemoryTest, initialization)
{
    givenMemoryInitialized();
    auto data = readDataFromMemory2();
    ASSERT_EQ(data, SharedMemoryData());
}

TEST_F(QtBasedSharedMemoryTest, serialization)
{
    givenMemoryInitialized();
    auto sourceData = whenWriteRandomDataToMemory1();
    auto targetData = readDataFromMemory2();
    ASSERT_EQ(sourceData, targetData);
}

} // namespace test
} // namespace nx::vms::client::desktop
