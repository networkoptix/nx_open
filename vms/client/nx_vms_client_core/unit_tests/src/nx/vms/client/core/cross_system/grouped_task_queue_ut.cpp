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

class GroupedTaskSequenceTest
{
public:
    MOCK_METHOD(void, taskStarted, (const QString& /*group*/, int /*id*/));
    MOCK_METHOD(void, taskFinished, (const QString& /*group*/, int /*id*/));
};

class GroupedTaskQueueTest: public Test
{
public:
    void taskFunction(GroupedTaskQueue::PromisePtr promise, const QString& group, int id);
    void addTask(const QString& group, int id);

    GroupedTaskQueue m_queue;
    GroupedTaskSequenceTest m_test;
    QEventLoop m_eventLoop;
};

void GroupedTaskQueueTest::taskFunction(
    GroupedTaskQueue::PromisePtr promise,
    const QString& group,
    int id)
{
    m_test.taskStarted(group, id);
    promise->finish();
    m_test.taskFinished(group, id);
}

void GroupedTaskQueueTest::addTask(const QString& group, int id)
{
    m_queue.add(std::make_shared<GroupedTaskQueue::Task>(
        [=, this](GroupedTaskQueue::PromisePtr promise) { taskFunction(promise, group, id); }),
        group);
}

TEST_F(GroupedTaskQueueTest, priorities)
{
    {
        InSequence sequence;

        EXPECT_CALL(m_test, taskStarted(QString("group1"), /*id*/ 1));
        EXPECT_CALL(m_test, taskFinished(QString("group1"), /*id*/ 1));

        EXPECT_CALL(m_test, taskStarted(QString("group1"), /*id*/ 2));
        EXPECT_CALL(m_test, taskFinished(QString("group1"), /*id*/ 2));

        EXPECT_CALL(m_test, taskStarted(QString("group2"), /*id*/ 3));
        EXPECT_CALL(m_test, taskFinished(QString("group2"), /*id*/ 3));

        EXPECT_CALL(m_test, taskStarted(QString("group2"), /*id*/ 4));
        EXPECT_CALL(m_test, taskFinished(QString("group2"), /*id*/ 4));

        EXPECT_CALL(m_test, taskStarted(QString("group3"), /*id*/ 5));
        EXPECT_CALL(m_test, taskFinished(QString("group3"), /*id*/ 5));

        EXPECT_CALL(m_test, taskStarted(QString("group3"), /*id*/ 6));
        EXPECT_CALL(m_test, taskFinished(QString("group3"), /*id*/ 6))
            .WillOnce([this]() { m_eventLoop.quit(); });
    }

    m_queue.setPriority("group1", 3);
    addTask("group3", /*id*/ 5);
    addTask("group1", /*id*/ 1);
    addTask("group1", /*id*/ 2);
    addTask("group3", /*id*/ 6);
    m_queue.setPriority("group2", 2);
    addTask("group2", /*id*/ 3);
    addTask("group2", /*id*/ 4);
    m_queue.setPriority("group3", 1);

    m_eventLoop.exec();
}

} // namespace nx::vms::client::core::test
