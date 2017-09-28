#include <gtest/gtest.h>
#include <memory>
#include <unordered_set>
#include <QtSql>
#include <nx/utils/test_support/test_options.h>
#include <nx/cloud/cdb/persistent_scheduler/persistent_scheduler.h>
#include <nx/cloud/cdb/persistent_scheduler/persistent_scheduler_db_helper.h>
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

        // destruction order is important
        scheduler.reset();
        executor.reset();
        dbHelper.reset();

        user1.reset();
        user2.reset();
    }

    void initDb()
    {
        if (dbPath.isEmpty())
        {
            dbPath = QDir(utils::TestOptions::temporaryDirectoryPath()).filePath("test_db.sqlite");
            QFile::remove(dbPath);
        }

        dbOptions.driverType = nx::utils::db::RdbmsDriverType::sqlite;
        dbOptions.dbName = dbPath;

        executor = std::make_unique<nx::utils::db::AsyncSqlQueryExecutor>(dbOptions);
        ASSERT_TRUE(executor->init());

        dbHelper = std::unique_ptr<SchedulerDbHelper>(new SchedulerDbHelper(executor.get()));
        ASSERT_TRUE(dbHelper->initDb());
    }

    void whenTwoUsersInitializedAndRegisteredToScheduler()
    {
        user1 = std::unique_ptr<SchedulerUser>(
            new SchedulerUser(
                executor.get(),
                scheduler.get(),
                scheduleUser1FunctorId));

        user1->registerAsAnEventReceiver();

        user2 = std::unique_ptr<SchedulerUser>(
            new SchedulerUser(
                executor.get(),
                scheduler.get(),
                scheduleUser2FunctorId));

        user2->registerAsAnEventReceiver();
    }

    void andWhenOnlyFirstUserRegistered()
    {
        user1 = std::unique_ptr<SchedulerUser>(
            new SchedulerUser(
                executor.get(),
                scheduler.get(),
                scheduleUser1FunctorId));

        user1->registerAsAnEventReceiver();
    }

    void andWhenFirstUsersUnsubscribesAllTasks()
    {
        nx::utils::promise<int> u1t1p;
        nx::utils::promise<int> u1t2p;

        auto u1t1f = u1t1p.get_future();
        auto u1t2f = u1t2p.get_future();

        ASSERT_FALSE(user1Task1Id.isNull());
        ASSERT_FALSE(user1Task2Id.isNull());

        user1->unsubscribe(
            user1Task1Id,
            [u1t1p = std::move(u1t1p)](const QnUuid&, const SchedulerUser::Task& task) mutable
            {
                u1t1p.set_value(task.fired);
            });

        user1->unsubscribe(
            user1Task2Id,
            [u1t2p = std::move(u1t2p)](const QnUuid&, const SchedulerUser::Task& task) mutable
            {
                u1t2p.set_value(task.fired);
            });

        user1Task1FiredWhileUnsubscribe = u1t1f.get();
        user1Task2FiredWhileUnsubscribe = u1t2f.get();
    }

    void andWhenFirstUsersUnsubscribesFirstTask()
    {
        nx::utils::promise<int> u1t1p;
        auto u1t1f = u1t1p.get_future();

        ASSERT_FALSE(user1Task1Id.isNull());

        user1->unsubscribe(
            user1Task1Id,
            [u1t1p = std::move(u1t1p)](const QnUuid&, const SchedulerUser::Task& task) mutable
            {
                u1t1p.set_value(task.fired);
            });

        user1Task1FiredWhileUnsubscribe = u1t1f.get();
    }

    void whenFirstUserInitializedAndRegisteredToScheduler()
    {
        user1 = std::unique_ptr<SchedulerUser>(
            new SchedulerUser(
                executor.get(),
                scheduler.get(),
                scheduleUser1FunctorId));

        user1->registerAsAnEventReceiver();
    }

    void whenFirstUserInitialized()
    {
        user1 = std::unique_ptr<SchedulerUser>(
            new SchedulerUser(
                executor.get(),
                scheduler.get(),
                scheduleUser1FunctorId));
    }

    void whenUserRegisteredAsReceiver()
    {
        user1->registerAsAnEventReceiver();
    }


    void whenFirstUserSchedulesALongTask()
    {
        whenFirstUserInitializedAndRegisteredToScheduler();
        const auto kLongTaskTimeout = std::chrono::seconds(3);

        nx::utils::promise<QnUuid> u1t1p;
        auto u1t1f = u1t1p.get_future();

        ASSERT_TRUE(user1Task1Id.isNull());


        user1->subscribe(
            kLongTaskTimeout,
            [u1t1p = std::move(u1t1p)](const QnUuid& taskId) mutable
            {
                u1t1p.set_value(taskId);
            });

        user1Task1Id = u1t1f.get();
    }

    void whenUserDecidesToSubscribeFromTimerFunction()
    {
        user1->setShouldSubscribe();
    }

    void whenUserDecidesToUnSubscribeFromTimerFunction()
    {
        user1->setShouldUnsubscribe(
            user1Task1Id,
            [this](const QnUuid& taskId, const SchedulerUser::Task& task)
            {
                unsubsribeCalledIds.emplace(taskId);
                user1Task1FiredWhileUnsubscribe = task.fired;
            });
    }

    void andWhenTimeToFireComesButServerIsOffline()
    {
        TearDown();
        qWarning() << "Server stopped";
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    void andWhenSchedulerWorksForSomeVeryShortTime()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    void andWhenTimeToNextScheduledFireComes()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }

    void whenTwoUsersScheduleTwoTasksEach()
    {
        whenTwoUsersInitializedAndRegisteredToScheduler();
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
        TearDown();
        SetUp();
    }

    void assertTasksFiredForUser(std::unique_ptr<SchedulerUser>& user)
    {
        auto userTasks = user->tasks();
        ASSERT_EQ(2U, userTasks.size());

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
        ASSERT_EQ(2U, user1Tasks.size());

        ASSERT_LE(user1Tasks[user1Task1Id].fired - user1Task1FiredWhileUnsubscribe, 1);
        ASSERT_LE(user1Tasks[user1Task2Id].fired - user1Task2FiredWhileUnsubscribe, 1);
    }

    void thenTasksShouldFireMultipleTimesForTheSecondUser()
    {
        assertTasksFiredForUser(user2);
    }

    void andNoTasksShouldFireForTheFirstUser()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(0U, user1Tasks.size());
    }

    void thenFirstUserShouldNotReceiveTimerEventsAnyLongerForTheFirstTask()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(2U, user1Tasks.size());

        ASSERT_LE(user1Tasks[user1Task1Id].fired - user1Task1FiredWhileUnsubscribe, 1);
    }

    void thenFirstUserShouldContinueToReceiveTimerEventsForTheSecondTask()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(2U, user1Tasks.size());

        ASSERT_GT(user1Tasks[user1Task2Id].fired, user1Task2FiredWhileUnsubscribe);
    }

    void thenSecondTaskShouldFireMultipleTimesForTheFirstUser()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(1U, user1Tasks.size());

        for (const auto& task: user1Tasks)
        {
            ASSERT_EQ(user1Task2Id, task.first);
            ASSERT_GT(task.second.fired, 0);
        }
    }

    void thenLongTaskNotFiredYet()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(1U, user1Tasks.size());

        for (const auto& task: user1Tasks)
            ASSERT_EQ(task.second.fired, 0);
    }

    void thenLongTaskShouldAtLastFire()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(1U, user1Tasks.size());

        for (const auto& task: user1Tasks)
            ASSERT_EQ(task.second.fired, 1);
    }

    void thenTaskShouldFireAsIfServerWasNotOffline()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(1U, user1Tasks.size());

        for (const auto& task: user1Tasks)
            ASSERT_EQ(task.second.fired, 2);
    }

    void thenFistTaskShouldFire()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(2U, user1Tasks.size());

        bool someTaskFiredExactlyTwice = std::any_of(
            user1Tasks.cbegin(),
            user1Tasks.cend(),
            [](const std::pair<QnUuid, SchedulerUser::Task> p)
            {
                return p.second.fired == 2;
            });
        ASSERT_TRUE(someTaskFiredExactlyTwice);
    }

    void thenSecondTaskShouldFireMultipleTimes()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(2U, user1Tasks.size());

        bool someTaskFiredMoreThan2 = std::any_of(
            user1Tasks.cbegin(),
            user1Tasks.cend(),
            [](const std::pair<QnUuid, SchedulerUser::Task> p)
            {
                return p.second.fired > 2;
            });
        ASSERT_TRUE(someTaskFiredMoreThan2);
    }

    void thenFirstTaskShouldFireAndSecondTaskShouldBeSubscribed()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(2U, user1Tasks.size());
    }

    void thenTaskShouldHaveAlreadyUnsubscribed()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_NE(unsubsribeCalledIds.find(user1Task1Id), unsubsribeCalledIds.cend());
    }

    void thenNoNewFires()
    {
        auto user1Tasks = user1->tasks();
        ASSERT_EQ(1U, user1Tasks.size());
        ASSERT_LE(user1Tasks[user1Task1Id].fired - user1Task1FiredWhileUnsubscribe, 1);
    }

    QString dbPath;
    nx::utils::db::ConnectionOptions dbOptions;
    std::unique_ptr<nx::utils::db::AsyncSqlQueryExecutor> executor;
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

    std::unordered_set<QnUuid> unsubsribeCalledIds;
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
    whenTwoUsersInitializedAndRegisteredToScheduler();
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

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_OneUnsubscribedAllTasks_AfterRestart_TwoUsersRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andWhenSchedulerWorksForSomeTime();
    thenTasksShouldFireMultipleTimesForBothUsers();

    andWhenFirstUsersUnsubscribesAllTasks();
    andWhenSchedulerWorksForSomeTime();
    andWhenSchedulerWorksForSomeTime();
    thenFirstUserShouldNotReceiveTimerEventsAnyLonger();

    andWhenSystemRestarts();
    whenTwoUsersInitializedAndRegisteredToScheduler();
    andWhenSchedulerWorksForSomeTime();

    thenTasksShouldFireMultipleTimesForTheSecondUser();
    andNoTasksShouldFireForTheFirstUser();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_OneUnsubscribedFirstTask_AfterRestart_TwoUsersRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andWhenSchedulerWorksForSomeTime();
    thenTasksShouldFireMultipleTimesForBothUsers();

    andWhenFirstUsersUnsubscribesFirstTask();
    andWhenSchedulerWorksForSomeTime();
    andWhenSchedulerWorksForSomeTime();
    thenFirstUserShouldNotReceiveTimerEventsAnyLongerForTheFirstTask();
    thenFirstUserShouldContinueToReceiveTimerEventsForTheSecondTask();

    andWhenSystemRestarts();
    whenTwoUsersInitializedAndRegisteredToScheduler();
    andWhenSchedulerWorksForSomeTime();

    thenTasksShouldFireMultipleTimesForTheSecondUser();
    thenSecondTaskShouldFireMultipleTimesForTheFirstUser();
}


TEST_F(SchedulerIntegrationTest, OneUserSchedulesLongTask_AfterRestartAndPause)
{
    whenFirstUserSchedulesALongTask();
    andWhenSchedulerWorksForSomeTime();
    andWhenSchedulerWorksForSomeTime();
    thenLongTaskNotFiredYet();

    andWhenSystemRestarts();
    whenFirstUserInitializedAndRegisteredToScheduler();
    andWhenSchedulerWorksForSomeTime();
    andWhenSchedulerWorksForSomeTime();
    thenLongTaskShouldAtLastFire();
}

TEST_F(SchedulerIntegrationTest, OneUserSchedulesLongTask_TaskTimeExpiresWhenServerIsOffline_AfterRestartAndPause)
{
    whenFirstUserSchedulesALongTask();
    andWhenSchedulerWorksForSomeTime();
    thenLongTaskNotFiredYet();

    andWhenTimeToFireComesButServerIsOffline();
    andWhenSystemRestarts();
    whenFirstUserInitializedAndRegisteredToScheduler();
    andWhenSchedulerWorksForSomeVeryShortTime();
    thenLongTaskShouldAtLastFire();

    andWhenTimeToNextScheduledFireComes();
    thenTaskShouldFireAsIfServerWasNotOffline();
}

TEST_F(SchedulerIntegrationTest, SubscribeFromExpiredTask)
{
    whenFirstUserSchedulesALongTask();
    andWhenSchedulerWorksForSomeTime();
    thenLongTaskNotFiredYet();

    andWhenTimeToFireComesButServerIsOffline();
    andWhenSystemRestarts();
    whenFirstUserInitializedAndRegisteredToScheduler();
    whenUserDecidesToSubscribeFromTimerFunction();
    andWhenSchedulerWorksForSomeVeryShortTime();
    thenFirstTaskShouldFireAndSecondTaskShouldBeSubscribed();

    andWhenTimeToNextScheduledFireComes();
    thenFistTaskShouldFire();
    thenSecondTaskShouldFireMultipleTimes();
}

TEST_F(SchedulerIntegrationTest, UnsubscribeFromExpiredTask)
{
    whenFirstUserSchedulesALongTask();
    andWhenSchedulerWorksForSomeTime();
    thenLongTaskNotFiredYet();

    andWhenTimeToFireComesButServerIsOffline();
    andWhenSystemRestarts();
    whenFirstUserInitialized();
    whenUserDecidesToUnSubscribeFromTimerFunction();
    whenUserRegisteredAsReceiver();
    andWhenSchedulerWorksForSomeVeryShortTime();
    thenTaskShouldHaveAlreadyUnsubscribed();

    andWhenTimeToNextScheduledFireComes();
    andWhenSchedulerWorksForSomeTime();
    thenNoNewFires();
}

} // namespace test
} // namespace cdb
} // namespace nx
