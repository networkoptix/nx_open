#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <media_server/media_server_module.h>
#include <nx/vms/event/actions/common_action.h>
#include <api/global_settings.h>
#include <api/helpers/event_log_request_data.h>

using namespace nx::vms::event;

namespace nx::vms::server::db::test
{

TEST(MaxEventLimit, maxEventLimit)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    const static int kEventLimit = 20;

    launcher.serverModule()->globalSettings()->setMaxEventLogRecords(kEventLimit);
    auto db = launcher.serverModule()->serverDb();

    for (int i = 0; i < kEventLimit * 2; ++i)
    {
        auto action = CommonAction::create(ActionType::bookmarkAction, EventParameters());
        db->saveActionToDB(action);
    }

    QnEventLogFilterData filter;
    filter.period.setEndTimeMs(QnTimePeriod::kMaxTimeValue);
    auto result = db->getActions(filter);
    ASSERT_LE(result.size(), kEventLimit * 1.2);
}

} // nx::vms::server::db::test
