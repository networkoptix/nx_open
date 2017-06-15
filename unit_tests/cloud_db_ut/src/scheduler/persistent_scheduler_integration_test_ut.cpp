#include <gtest/gtest.h>
#include <memory>
#include <QtSql>
#include <nx/utils/test_support/test_options.h>
#include <persistent_scheduler/persistent_scheduler.h>
#include <persistent_scheduler/persistent_scheduler_db_helper.h>
#include "scheduler_user.h"

namespace nx {
namespace cdb {
namespace test {

static const QnUuid scheduleUser1FunctorId = QnUuid::createUuid();
static const QnUuid scheduleUser2FunctorId = QnUuid::createUuid();

static const std::chrono::milliseconds kFirstUserFirstTaskTimeout(100);
static const std::chrono::milliseconds kFirstUserSecondTaskTimeout(200);

static const std::chrono::milliseconds kSecondUserFirstTaskTimeout(150);
static const std::chrono::milliseconds kSecondUserSecondTaskTimeout(300);

class SchedulerIntegrationTest : public ::testing::Test
{
protected:

    virtual void SetUp() override
    {
        initDb();

        scheduler = std::unique_ptr<PersistentScheduler>(
            new PersistentScheduler(executor.get(), dbHelper.get()));
        scheduler->start();
    }

    virtual void TearDown() override
    {
        if (scheduler)
            scheduler->stop();
    }

    void initDb()
    {
        if (dbPath.isEmpty())
        {
            dbPath = QDir(utils::TestOptions::temporaryDirectoryPath()).filePath("test_db.sqlite");
            QFile::remove(dbPath);
        }

        dbOptions.driverType = db::RdbmsDriverType::sqlite;
        dbOptions.dbName = dbPath;

        executor = std::unique_ptr<db::AsyncSqlQueryExecutor>(new db::AsyncSqlQueryExecutor(dbOptions));
        ASSERT_TRUE(executor->init());

        dbHelper = std::unique_ptr<SchedulerDbHelper>(new SchedulerDbHelper(executor.get()));
        ASSERT_TRUE(dbHelper->initDb());
    }

    void whenTwoUsersInitializedAndRegistredToScheduler()
    {
        user1 = std::unique_ptr<SchedulerUser>(
            new SchedulerUser(
                executor.get(),
                scheduler.get(),
                scheduleUser1FunctorId));

        user2 = std::unique_ptr<SchedulerUser>(
            new SchedulerUser(
                executor.get(),
                scheduler.get(),
                scheduleUser2FunctorId));
    }

    void andWhenOnlyFirstUserRegistered()
    {
        user1 = std::unique_ptr<SchedulerUser>(
            new SchedulerUser(
                executor.get(),
                scheduler.get(),
                scheduleUser1FunctorId));
    }

    void andWhenFirstUsersUnsubscribes()
    {
        nx::utils::promise<int> u1t1p;
        nx::utils::promise<int> u1t2p;

        auto u1t1f = u1t1p.get_future();
        auto u1t2f = u1t2p.get_future();

        ASSERT_FALSE(user1Task1Id.isNull());
        ASSERT_FALSE(user1Task2Id.isNull());

        user1->unsubscribe(
            user1Task1Id,
            [u1t1p = std::move(u1t1p)](const SchedulerUser::Task& task) mutable
            {
                u1t1p.set_value(task.fired);
            });

        user1->unsubscribe(
            user1Task2Id,
            [u1t2p = std::move(u1t2p)](const SchedulerUser::Task& task) mutable
            {
                u1t2p.set_value(task.fired);
            });

        user1Task1FiredWhileUnsubscribe = u1t1f.get();
        user1Task2FiredWhileUnsubscribe = u1t2f.get();
    }

    void whenTwoUsersScheduleTwoTasksEach()
    {
        whenTwoUsersInitializedAndRegistredToScheduler();
        nx::utils::promise<QnUuid> u1t1p;
        nx::utils::promise<QnUuid> u1t2p;
        nx::utils::promise<QnUuid> u2t1p;
        nx::utils::promise<QnUuid> u2t2p;

        auto u1t1f = u1t1p.get_future();
        auto u1t2f = u1t2p.get_future();
        auto u2t1f = u2t1p.get_future();
        auto u2t2f = u2t2p.get_future();

        user1->subscribe(
            kFirstUserFirstTaskTimeout,
            [u1t1p = std::move(u1t1p)](const QnUuid& id) mutable { u1t1p.set_value(id); });

        user1->subscribe(
            kFirstUserSecondTaskTimeout,
            [u1t2p = std::move(u1t2p)](const QnUuid& id) mutable { u1t2p.set_value(id); });

        user2->subscribe(
            kSecondUserFirstTaskTimeout,
            [u2t1p = std::move(u2t1p)](const QnUuid& id) mutable { u2t1p.set_value(id); });

        user2->subscribe(
            kSecondUserSecondTaskTimeout,
            [u2t2p = std::move(u2t2p)](const QnUuid& id) mutable { u2t2p.set_value(id); });


        user1Task1Id = u1t1f.get();
        user1Task2Id = u1t2f.get();
        user2Task1Id = u2t1f.get();
        user2Task2Id = u2t2f.get();
    }

    void andWhenSchedulerWorksForSomeTime()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    void andWhenSystemRestarts()
    {
        scheduler->stop();
        user1.reset();
        user2.reset();
        scheduler.reset();
        dbHelper.reset();
        executor.reset();

        SetUp();
    }

    void assertTasksFiredForUser(std::unique_ptr<SchedulerUser>& user)
    {
        auto userTasks = user->tasks();
        ASSERT_EQ(userTasks.size(), 2);

        for (const auto& task: userTasks)
            ASSERT_GT(task.second.fired, 0);
    }

    void thenTasksShouldFireMultipleTimesForBothUsers()
    {
        assertTasksFiredForUser(user1);
        assertTasksFiredForUser(user2);
    }

    void thenTasksShouldFireMultipleTimesForTheFirstUser()
    {
        assertTasksFiredForUser(user1);
    }

    void thenSchedulerShouldBeIdleWithoutCrashes()
    {
        andWhenSchedulerWorksForSomeTime();
    }

    void thenFirstUserShouldNotReceiveTimerEventsAnyLonger()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(user1Tasks.size(), 2);

        ASSERT_EQ(user1Tasks[user1Task1Id].fired, user1Task1FiredWhileUnsubscribe);
        ASSERT_EQ(user1Tasks[user1Task2Id].fired, user1Task2FiredWhileUnsubscribe);
    }

    void thenTasksShouldFireMultipleTimesForTheSecondUser()
    {
        assertTasksFiredForUser(user2);
    }

    void andNoTasksShouldFireForTheFirstUser()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(user1Tasks.size(), 0);
    }

    QString dbPath;
    db::ConnectionOptions dbOptions;
    std::unique_ptr<db::AsyncSqlQueryExecutor> executor;
    std::unique_ptr<SchedulerDbHelper> dbHelper;
    std::unique_ptr<PersistentScheduler> scheduler;
    std::unique_ptr<SchedulerUser> user1;
    std::unique_ptr<SchedulerUser> user2;

    QnUuid user1Task1Id;
    QnUuid user1Task2Id;
    QnUuid user2Task1Id;
    QnUuid user2Task2Id;

    int user1Task1FiredWhileUnsubscribe = 0;
    int user1Task2FiredWhileUnsubscribe = 0;
};

TEST_F(SchedulerIntegrationTest, TwoUsersScheduleTasks)
{
    whenTwoUsersScheduleTwoTasksEach();
    andWhenSchedulerWorksForSomeTime();
    thenTasksShouldFireMultipleTimesForBothUsers();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_AfterRestart_NoUsersRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andWhenSchedulerWorksForSomeTime();
    andWhenSystemRestarts();

    thenSchedulerShouldBeIdleWithoutCrashes();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_AfterRestart_TwoUsersRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andWhenSchedulerWorksForSomeTime();

    andWhenSystemRestarts();
    whenTwoUsersInitializedAndRegistredToScheduler();
    andWhenSchedulerWorksForSomeTime();

    thenTasksShouldFireMultipleTimesForBothUsers();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_AfterRestart_OneUserRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andWhenSchedulerWorksForSomeTime();

    andWhenSystemRestarts();
    andWhenOnlyFirstUserRegistered();
    andWhenSchedulerWorksForSomeTime();

    thenTasksShouldFireMultipleTimesForTheFirstUser();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_OneUnsubscribed_AfterRestart_TwoUsersRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andWhenSchedulerWorksForSomeTime();
    thenTasksShouldFireMultipleTimesForBothUsers();

    andWhenFirstUsersUnsubscribes();
    andWhenSchedulerWorksForSomeTime();
    andWhenSchedulerWorksForSomeTime();
    thenFirstUserShouldNotReceiveTimerEventsAnyLonger();

    andWhenSystemRestarts();
    whenTwoUsersInitializedAndRegistredToScheduler();
    andWhenSchedulerWorksForSomeTime();

    thenTasksShouldFireMultipleTimesForTheSecondUser();
    andNoTasksShouldFireForTheFirstUser();
}

//TEST_F(SchedulerIntegrationTest, OneUserSchedulesLongTask_AfterRestartAndPause)
//{
//    whenTwoUsersScheduleTwoTasksEach();
//    thenTasksShouldFireMultipleTimes();
//}

}
}
}
