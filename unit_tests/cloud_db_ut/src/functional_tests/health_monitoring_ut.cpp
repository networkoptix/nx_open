#include <chrono>
#include <thread>

#include <nx/utils/sync_call.h>

#include "health_monitoring_fixture.h"

namespace nx {
namespace cdb {
namespace test {

TEST_F(FtHealthMonitoring, system_status_is_correct)
{
    for (int i = 0; i < 2; ++i)
    {
        assertSystemOffline();
        establishConnectionFromMediaserverToCloud();
        assertSystemOnline();

        if (i == 0)
        {
            closeConnectionFromMediaserverToCloud();
            whenCdbIsRestarted();
        }
    }
}

TEST_F(FtHealthMonitoring, history_is_persistent)
{
    establishConnectionFromMediaserverToCloud();
    closeConnectionFromMediaserverToCloud();

    assertHistoryIsCorrect();

    whenCdbIsRestarted();

    assertHistoryIsCorrect();
}

TEST_F(FtHealthMonitoring, history_is_not_reported_for_unknown_id)
{
    api::SystemHealthHistory history;
    ASSERT_NE(
        api::ResultCode::ok,
        getSystemHealthHistory(
            account().email, account().password,
            QnUuid::createUuid().toStdString(), &history));
}

TEST_F(FtHealthMonitoring, history_is_available_to_system_owner_only)
{
    givenSystemWithSomeHistory();
    whenSystemIsSharedWithSomeone();
    thenSomeoneDoesNotHaveAccessToTheHistory();
}

TEST_F(FtHealthMonitoring, history_is_not_available_to_system_credentials)
{
    givenSystemWithSomeHistory();
    thenSystemCredentialsCannotBeUsedToAccessHistory();
}

TEST_F(FtHealthMonitoring, no_excess_system_online_records)
{
    establishConnectionFromMediaserverToCloud();
    assertHistoryHasASingleOnlineRecord();

    establishConnectionFromMediaserverToCloudReusingPeerId();

    // Giving some for history to be saved to DB.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    assertHistoryHasASingleOnlineRecord();
}

} // namespace test
} // namespace cdb
} // namespace nx
