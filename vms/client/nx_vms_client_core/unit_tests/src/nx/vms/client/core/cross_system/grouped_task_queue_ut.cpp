// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <QtCore/QEventLoop>
#include <QtCore/QString>
#include <QtCore/QTimer>

#include <nx/vms/client/core/cross_system/private/grouped_task_queue.h>

namespace nx::vms::client::core::test {
using namespace testing;

class GroupRequestInvokeSequenceTest
{
public:
    MOCK_METHOD(void, requestInvoked, (const QString& /*group*/, int /*id*/));
    MOCK_METHOD(void, requestFinished, (const QString& /*group*/, int /*id*/));
};

class GroupQueueTest: public Test
{
public:
    static constexpr auto kTimeout = std::chrono::seconds(7);

    virtual void SetUp() override;
    void requestFunction(GroupedTaskQueue::PromisePtr promise, const QString& group, int id);
    void addRequest(const QString& group, int id);

    GroupedTaskQueue m_queue;
    GroupRequestInvokeSequenceTest m_test;
    QEventLoop m_eventLoop;
};

void GroupQueueTest::SetUp()
{
    QTimer::singleShot(kTimeout, &m_eventLoop,
        [this]()
        {
            m_eventLoop.quit();
            ASSERT_TRUE(false) << "Timeout";
        });
}

void GroupQueueTest::requestFunction(GroupedTaskQueue::PromisePtr promise, const QString& group, int id)
{
    m_test.requestInvoked(group, id);

    std::thread(
        [this, promise, group, id]()
        {
            m_test.requestFinished(group, id);
            promise->finish();
        }).detach();
}

void GroupQueueTest::addRequest(const QString& group, int id)
{
    m_queue.add(std::make_shared<GroupedTaskQueue::Task>(
        [=, this](GroupedTaskQueue::PromisePtr promise) { requestFunction(promise, group, id); }),
        group);
}

TEST_F(GroupQueueTest, priorities)
{
    {
        InSequence sequence;

        EXPECT_CALL(m_test, requestInvoked(QString("group1"), /*id*/ 1));
        EXPECT_CALL(m_test, requestFinished(QString("group1"), /*id*/ 1));

        EXPECT_CALL(m_test, requestInvoked(QString("group1"), /*id*/ 2));
        EXPECT_CALL(m_test, requestFinished(QString("group1"), /*id*/ 2));

        EXPECT_CALL(m_test, requestInvoked(QString("group2"), /*id*/ 3));
        EXPECT_CALL(m_test, requestFinished(QString("group2"), /*id*/ 3));

        EXPECT_CALL(m_test, requestInvoked(QString("group2"), /*id*/ 4));
        EXPECT_CALL(m_test, requestFinished(QString("group2"), /*id*/ 4));

        EXPECT_CALL(m_test, requestInvoked(QString("group3"), /*id*/ 5));
        EXPECT_CALL(m_test, requestFinished(QString("group3"), /*id*/ 5));

        EXPECT_CALL(m_test, requestInvoked(QString("group3"), /*id*/ 6));
        EXPECT_CALL(m_test, requestFinished(QString("group3"), /*id*/ 6))
            .WillOnce([this]() { m_eventLoop.quit(); });
    }

    m_queue.setPriority("group1", 3);
    addRequest("group3", /*id*/ 5);
    addRequest("group1", /*id*/ 1);
    addRequest("group1", /*id*/ 2);
    addRequest("group3", /*id*/ 6);
    m_queue.setPriority("group2", 2);
    addRequest("group2", /*id*/ 3);
    addRequest("group2", /*id*/ 4);
    m_queue.setPriority("group3", 1);

    m_eventLoop.exec();
}

} // namespace nx::vms::client::core::test
