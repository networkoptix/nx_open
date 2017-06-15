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

    void whenTwoUsersScheduleTwoTasksEach()
    {
        whenTwoUsersInitializedAndRegistredToScheduler();

        user1->subscribe(kFirstUserFirstTaskTimeout);
        user1->subscribe(kFirstUserSecondTaskTimeout);
        user2->subscribe(kSecondUserFirstTaskTimeout);
        user2->subscribe(kSecondUserSecondTaskTimeout);
    }

    void andWhenSchedulerWorksForSomeTime(int timeoutMs)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(timeoutMs));
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
        {
            ASSERT_GT(task.second.fired, 0);
            ASSERT_TRUE(task.second.subscribed);
        }
    }

    void thenTasksShouldFireMultipleTimesForBothUsers()
    {
        assertTasksFiredForUser(user1);
        assertTasksFiredForUser(user2);
    }

    void thenSchedulerShouldBeIdleWithoutCrashes()
    {
        andWhenSchedulerWorksForSomeTime(1000);
    }

    QString dbPath;
    db::ConnectionOptions dbOptions;
    std::unique_ptr<db::AsyncSqlQueryExecutor> executor;
    std::unique_ptr<SchedulerDbHelper> dbHelper;
    std::unique_ptr<PersistentScheduler> scheduler;
    std::unique_ptr<SchedulerUser> user1;
    std::unique_ptr<SchedulerUser> user2;
};

TEST_F(SchedulerIntegrationTest, TwoUsersScheduleTasks)
{
    whenTwoUsersScheduleTwoTasksEach();
    andWhenSchedulerWorksForSomeTime(1000);
    thenTasksShouldFireMultipleTimesForBothUsers();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_AfterRestart_NoUsersRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andWhenSchedulerWorksForSomeTime(500);
    andWhenSystemRestarts();

    thenSchedulerShouldBeIdleWithoutCrashes();
}

//TEST_F(SchedulerIntegrationTest, TwoUsersTasks_AfterRestart_TwoUsersRegistered)
//{
//    whenTwoUsersScheduleTwoTasksEach();
//    andSystemRestarts();
//    andBothUsersRegistred();
//
//    thenTasksShouldFireMultipleTimesForBothUsers();
//}
//
//TEST_F(SchedulerIntegrationTest, TwoUsersTasks_AfterRestart_OneUserRegistered)
//{
//    whenTwoUsersScheduleTwoTasksEach();
//    andSystemRestarts();
//    andOnlyOneUserRegistred();
//
//    thenTasksShouldFireMultipleTimesForTheFirstUser();
//}
//
//TEST_F(SchedulerIntegrationTest, TwoUsersTasks_OneUnsubscribed_AfterRestart_TwoUsersRegistered)
//{
//    whenTwoUsersScheduleTwoTasksEach();
//    andFirstUsersUnsubscribes();
//    thenFirstUserShouldNotReceiveTimerEventsAnyLonger();
//
//    andSystemRestarts();
//    andBothUsersRegistred();
//
//    thenTasksShouldFireMultipleTimesForTheSecondUser();
//    andNoTasksShouldFireForTheFirstUser();
//}
//
//TEST_F(SchedulerIntegrationTest, OneUserSchedulesLongTask_AfterRestartAndPause_)
//{
//    whenTwoUsersScheduleTwoTasksEach();
//    thenTasksShouldFireMultipleTimes();
//}

}
}
}