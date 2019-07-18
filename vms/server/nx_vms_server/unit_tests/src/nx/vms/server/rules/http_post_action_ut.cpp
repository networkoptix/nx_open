#include <gtest/gtest.h>

#include <test_support/mediaserver_launcher.h>
#include <media_server/media_server_module.h>
#include <nx/vms/event/actions/common_action.h>
#include <api/helpers/event_log_request_data.h>
#include <api/test_api_requests.h>
#include <rest/server/json_rest_result.h>
#include <nx/network/url/url_builder.h>

using namespace nx::vms::event;
using namespace nx::test;

namespace nx::vms::server::event::test
{

TEST(EventRules, httpPostAction)
{
    MediaServerLauncher launcher;
    ASSERT_TRUE(launcher.start());

    auto connection = launcher.serverModule()->ec2Connection();
    nx::vms::api::EventRuleData rule;

    rule.eventType = EventType::userDefinedEvent;
    rule.actionType = ActionType::execHttpRequestAction;
    EventParameters eventParameters;
    eventParameters.caption = "123";
    rule.eventCondition = QJson::serialized(eventParameters);
    ActionParameters actionParams;
    actionParams.url = nx::network::url::Builder(launcher.apiUrl())
        .setUserName("admin")
        .setPassword("admin")
        .setPath("/ec2/saveUser").toString();

    actionParams.text = R"json(
        {"name": "liver", "password":"LiveLive123", "isLdap": false, "isEnabled": true}
    )json";
    rule.actionParams = QJson::serialized(actionParams);

    auto dbError = connection->getEventRulesManager(Qn::kSystemAccess)->saveSync(rule);
    ASSERT_EQ(ec2::ErrorCode::ok, dbError);

    QnJsonRestResult result;
    NX_TEST_API_GET(&launcher, "/api/createEvent?event_type=userDefinedEvent&caption=123", &result);

    bool newUserFound = false;
    nx::vms::api::UserDataList users;

    do
    {
        dbError = connection->getUserManager(Qn::kSystemAccess)->getUsersSync(&users);
        ASSERT_EQ(ec2::ErrorCode::ok, dbError);
        for (const auto& user: users)
            newUserFound |= user.name == "liver";
        if (!newUserFound)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
    } while (!newUserFound);
    ASSERT_TRUE(newUserFound);
}

} // nx::vms::server::db::test
