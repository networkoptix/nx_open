#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <media_server/media_server_module.h>
#include <api/helpers/event_log_request_data.h>
#include <nx/vms/event/actions/common_action.h>

using namespace nx::vms::event;

namespace nx::vms::server::db::test
{

TEST(ServerStartedEvent, serverStartedEvent)
{
    MediaServerLauncher launcher;
    for (int i = 1; i <= 2; ++i)
    {
        ASSERT_TRUE(launcher.start());
        QnEventLogFilterData filter;
        filter.eventType = EventType::serverStartEvent;
        filter.period.setEndTimeMs(QnTimePeriod::kMaxTimeValue);

        auto db = launcher.serverModule()->serverDb();
        do
        {
            auto result = db->getActions(filter);
            if (i == result.size())
                break;

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        while (true);

        launcher.stop();
    }
}

} // nx::vms::server::db::test
