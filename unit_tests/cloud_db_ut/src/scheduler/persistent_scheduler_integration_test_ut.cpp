#include <gtest/gtest.h>
#include <persistent_scheduler/persistent_scheduler.h>
#include <persistent_scheduler/persistent_scheduler_db_helper.h>
#include "scheduler_user.h"

namespace nx {
namespace cdb {
namespace test {

static const QnUuid scheduleUser1FunctorId = QnUuid::createUuid();
static const QnUuid scheduleUser2FunctorId = QnUuid::createUuid();

class SchedulerIntegrationTest : public ::testing::Test
{
protected:

    std::unique_ptr<SchedulerUser> user1;
    std::unique_ptr<SchedulerUser> user2;
};

TEST_F(SchedulerIntegrationTest, TwoUsersScheduleTasks)
{
    whenTwoUsersScheduleTwoTasksEach();
    thenTasksShouldFireMultipleTimes();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_AfterRestart_NoUsersRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andSystemRestarts();
    andNoUsersRegistred();

    thenSchedulerShouldBeIdleWithoutCrashes();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_AfterRestart_TwoUsersRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andSystemRestarts();
    andBothUsersRegistred();

    thenTasksShouldFireMultipleTimesForBothUsers();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_AfterRestart_OneUserRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andSystemRestarts();
    andOnlyOneUserRegistred();

    thenTasksShouldFireMultipleTimesForTheFirstUser();
}

TEST_F(SchedulerIntegrationTest, TwoUsersTasks_OneUnsubscribed_AfterRestart_TwoUsersRegistered)
{
    whenTwoUsersScheduleTwoTasksEach();
    andFirstUsersUnsubscribes();
    thenFirstUserShouldNotReceiveTimerEventsAnyLonger();

    andSystemRestarts();
    andBothUsersRegistred();

    thenTasksShouldFireMultipleTimesForTheSecondUser();
    andNoTasksShouldFireForTheFirstUser();
}

}
}
}