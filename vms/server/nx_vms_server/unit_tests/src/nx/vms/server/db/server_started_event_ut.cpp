#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <media_server/media_server_module.h>
#include <api/helpers/event_log_request_data.h>
#include <nx/vms/event/actions/common_action.h>
#include <nx/vms/server/event/extended_rule_processor.h>

using namespace nx::vms::event;

namespace nx::vms::server::db::test
{

TEST(ServerStartedEvent, serverStartedEvent)
{
    MediaServerLauncher launcher;
    launcher.addSetting(QnServer::serverStartedEventTimeoutMsName, "1");
    for (int i = 1; i <= 2; ++i)
    {
        ASSERT_TRUE(launcher.start());
        launcher.serverModule()->eventRuleProcessor()->waitForDone();

        QnEventLogFilterData filter;
        filter.eventType = EventType::serverStartEvent;
        filter.period.setEndTimeMs(QnTimePeriod::kMaxTimeValue);
        auto db = launcher.serverModule()->serverDb();

        nx::utils::ElapsedTimer timer;
        timer.restart();
        vms::event::ActionDataList result;
        while(1)
        {
            result = db->getActions(filter);
            if (result.size() == i || timer.hasExpired(std::chrono::seconds(10)))
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        };

        ASSERT_EQ(i, result.size());

        launcher.stop();
    }
}

} // nx::vms::server::db::test
