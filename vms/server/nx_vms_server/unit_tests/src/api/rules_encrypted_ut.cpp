#include <memory>
#include <gtest/gtest.h>

#include <rest/server/json_rest_result.h>
#include <server_query_processor.h>
#include <nx/network/http/http_types.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/types/event_rule_types.h>
#include <nx/vms/event/action_parameters.h>

#include "test_api_requests.h"

namespace nx::test {


static const QString kTestPassword = "123";

class EventRuleEncryptFt: public ::testing::Test
{
protected:
    EventRuleEncryptFt()
    {
        m_server = std::unique_ptr<MediaServerLauncher>(new MediaServerLauncher());
    }

    void whenServerLaunched()
    {
        ASSERT_TRUE(m_server->start());
    }

    void whenServerLaunchedAndDataPrepared()
    {
        whenServerLaunched();

        nx::vms::event::ActionParameters params;
        auto url = nx::utils::Url("http://localhost/ec2/test");
        url.setUserName("admin");
        url.setPassword(kTestPassword);
        params.url = url.toString();
        m_eventRule.id = QnUuid::createUuid();
        m_eventRule.actionParams = QJson::serialized(params);
        m_eventRule.eventType = nx::vms::api::EventType::cameraMotionEvent;
        m_eventRule.actionType = nx::vms::api::ActionType::execHttpRequestAction;

        NX_TEST_API_POST(m_server.get(), "/ec2/saveEventRule", m_eventRule);

        m_nonAdminUser.name = "user1";
        m_nonAdminUser.password = "111";
        m_nonAdminUser.isAdmin = false;
        NX_TEST_API_POST(m_server.get(), "/ec2/saveUser", m_nonAdminUser);
    }

    void thenAdminCanSeePassword()
    {
        nx::vms::api::EventRuleDataList eventRules;
        NX_TEST_API_GET(m_server.get(), "/ec2/getEventRules", &eventRules);
        thenPasswordShouldBe(eventRules, kTestPassword);
    }

    void thenNonAdminCanNotSeePassword()
    {
        nx::vms::api::EventRuleDataList eventRules;
        NX_TEST_API_GET(m_server.get(), "/ec2/getEventRules", &eventRules,
            nx::network::http::StatusCode::ok,
            m_nonAdminUser.name, m_nonAdminUser.password);
        thenPasswordShouldBe(eventRules, ec2::kHiddenPasswordFiller);

        nx::vms::api::FullInfoData fullInfoData;
        NX_TEST_API_GET(m_server.get(), "/ec2/getFullInfo", &fullInfoData,
            nx::network::http::StatusCode::ok,
            m_nonAdminUser.name, m_nonAdminUser.password);
        thenPasswordShouldBe(fullInfoData.rules, ec2::kHiddenPasswordFiller);

    }

    void thenPasswordShouldBe(const nx::vms::api::EventRuleDataList& eventRules, const QString& password)
    {
        bool isRuleFound = false;
        for (const auto& rule: eventRules)
        {
            if (rule.id != m_eventRule.id)
                continue;
            auto params = QJson::deserialized<nx::vms::event::ActionParameters>(rule.actionParams);
            auto url = nx::utils::Url(params.url);
            isRuleFound = true;
            ASSERT_EQ(password, url.password());
        }
        ASSERT_TRUE(isRuleFound);
    }

    void thenServerSeeDataUnencrypted()
    {
        const auto resourcePool = m_server->commonModule()->resourcePool();
        const auto rules = resourcePool->eventRuleManager()->rules();
        nx::vms::api::EventRuleDataList apiRuleList;
        ec2::fromResourceListToApi(rules, apiRuleList);
        thenPasswordShouldBe(apiRuleList, kTestPassword);
    }

private:
    std::unique_ptr<MediaServerLauncher> m_server;
    vms::api::UserDataEx m_nonAdminUser;
    vms::api::EventRuleData m_eventRule;
};

TEST_F(EventRuleEncryptFt, amendTransaction)
{
    whenServerLaunchedAndDataPrepared();

    thenAdminCanSeePassword();
    thenNonAdminCanNotSeePassword();

    thenServerSeeDataUnencrypted();
}

} // namespace nx::test
