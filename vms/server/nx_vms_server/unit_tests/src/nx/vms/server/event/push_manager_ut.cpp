#include <gtest/gtest.h>

#include <api/global_settings.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>
#include <media_server/media_server_module.h>
#include <nx/fusion/model_functions.h>
#include <nx/mediaserver/camera_mock.h>
#include <nx/network/http/test_http_server.h>
#include <nx/utils/thread/sync_queue.h>
#include <nx/vms/event/actions/abstract_action.h>
#include <nx/vms/event/actions/common_action.h>
#include <nx/vms/event/event_parameters.h>
#include <nx/vms/server/event/push_manager.h>
#include <nx/vms/utils/app_info.h>

namespace nx::vms::server::event::test {

using namespace nx::network;
using namespace nx::vms::event;
using namespace nx::vms::server::event;
using namespace std::chrono_literals;

static const QString kMailSuffix("@xxx.com");
static const std::chrono::milliseconds kQueueTimeout(100);

class PushManagerTest: public ::testing::Test
{
public:
    QString systemId;
    QnUuid serverId;
    QnUuid cameraId;

public:
    PushManagerTest(RetryPolicy retryPolicy = {0, 0s, 0, 0s, 0}):
        systemId(QnUuid::createUuid().toSimpleString()),
        serverId(QnUuid::createUuid().toSimpleString()),
        m_pushManager(
            &m_serverModule,
            RetryPolicy(1, kQueueTimeout * 3, 1, kQueueTimeout * 3, 0),
            /*useEncryption*/ false)
    {
        m_cloudService.registerRequestProcessorFunc(
            "/api/notifications/push_notification",
            [this](auto context, auto handler) { handler(processCloudRequest(context.request)); });
        NX_ASSERT(m_cloudService.bindAndListen());

        const auto settings = m_serverModule.commonModule()->globalSettings();
        settings->setCloudSystemId(systemId);
        settings->setCloudHost(m_cloudService.serverAddress().toString());

        // Initialize push notifications language.
        m_pushManager.updateTargetLanguage();

        QnMediaServerResourcePtr server(new QnMediaServerResource(m_serverModule.commonModule()));
        server->setIdUnsafe(serverId);
        server->setName("My Cool Server");
        m_serverModule.commonModule()->resourcePool()->addResource(server);

        QnSharedResourcePointer<resource::test::CameraMock> camera(
            new resource::test::CameraMock(&m_serverModule));
        camera->setName("My Cool Camera");
        cameraId = camera->getId();
        m_serverModule.commonModule()->resourcePool()->addResource(camera);

        for (const QString& u: {"a", "b", "c"})
        {
            QnUserResourcePtr user(new QnUserResource(QnUserType::Cloud));
            user->setIdUnsafe(QnUuid::createUuid());
            user->setName(u + kMailSuffix);
            user->setRawPermissions(vms::api::GlobalPermission::adminPermissions);
            m_serverModule.commonModule()->resourcePool()->addResource(user);
            m_users[u] = user;
        }
    }

    enum GenerateOptions
    {
        noOptions = 0,
        addSource = 1 << 0,
        cameraRef = 1 << 1,
    };

    void generateEvent(
        EventType type,
        QnUuid resource = {},
        const std::string& users = "",
        QString caption = {},
        QString body = {},
        GenerateOptions options = addSource)
    {
        EventParameters event;
        event.eventType = type;
        event.eventTimestampUsec = 111222333444555;
        if (options & cameraRef)
            event.metadata.cameraRefs.push_back(resource.toString());
        else
            event.eventResourceId = std::move(resource);

        ActionParameters action;
        if (users.empty())
        {
            action.allUsers = true;
        }
        else
        {
            for (const auto u: users)
                action.additionalResources.push_back(m_users[QString(u)]->getId());
        }
        action.sayText = std::move(caption);
        action.text = std::move(body);
        action.useSource = bool(options & addSource);

        AbstractActionPtr actionPtr(new CommonAction(ActionType::pushNotificationAction, event));
        actionPtr->setParams(action);
        m_pushManager.send(actionPtr);
    }

    std::optional<PushRequest> getRequestFromServer()
    {
        if (const auto request = m_cloudRequests.pop(boost::make_optional(kQueueTimeout)))
            return QJson::deserialized<PushRequest>(*request);
        return std::nullopt;
    }

    std::optional<PushRequest> testEvent(
        EventType type,
        QnUuid resource = {},
        const std::string& users = "",
        QString caption = {},
        QString body = {},
        GenerateOptions options = addSource)
    {
        generateEvent(type, resource, users, caption, body, options);
        return getRequestFromServer();
    }

    void setCloudState(http::StatusCode::Value value)
    {
        m_cloudState = value;
        NX_INFO(this, "Cloud state changed: %1", value);
    }

    QString openUrl(const QnUuid& resource = {}) const
    {
        const auto settings = m_serverModule.commonModule()->globalSettings();
        QString url = NX_FMT("%1://%2/client/%3/view?timestamp=111222333444",
            nx::vms::utils::AppInfo::nativeUriProtocol(), settings->cloudHost(), systemId);
        if (!resource.isNull())
            url += "&resources=" + resource.toSimpleString();
        return url;
    }

    QString imageUrl(const QnUuid& resource = {}) const
    {
        if (resource.isNull())
            return QString();
        return NX_FMT("https://%1/ec2/cameraThumbnail?%2&cameraId=%3&time=111222333444",
            systemId,
            "height=480&streamSelectionMode=forcedSecondary&method=precise",
            resource.toSimpleString());
    }

    void setUserPermissions(const std::string& users, vms::api::GlobalPermission permissions)
    {
        for (const auto u: users)
            m_users[QString(u)]->setRawPermissions(permissions);
    }

private:
    http::StatusCode::Value processCloudRequest(const http::Request& request)
    {
        const auto cloudState = m_cloudState.load();
        NX_DEBUG(this, "%1 -> %2", request.messageBody, cloudState);
        if (cloudState != http::StatusCode::created)
            return cloudState;

        m_cloudRequests.push(request.messageBody);
        return http::StatusCode::created;
    }

private:
    QnMediaServerModule m_serverModule;
    PushManager m_pushManager;
    std::map<QString, QnUserResourcePtr> m_users;

    nx::utils::SyncQueue<Buffer> m_cloudRequests;
    std::atomic<http::StatusCode::Value> m_cloudState{http::StatusCode::created};
    http::TestHttpServer m_cloudService;
};

#define EXPECT_LIST(VALUE, EXPECTED) \
    EXPECT_EQ(containerString(VALUE), containerString<std::vector<QString>> EXPECTED)

TEST_F(PushManagerTest, MessageCaption)
{
    // Default.
    {
        const auto sent = testEvent(EventType::userDefinedEvent, {}, {}, "", "", noOptions);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com","b@xxx.com","c@xxx.com"}));
        EXPECT_EQ(sent->systemId, systemId);
        EXPECT_EQ(sent->notification.title, "Generic Event");
        EXPECT_EQ(sent->notification.body, "");
        EXPECT_EQ(sent->notification.payload.url, openUrl());
        EXPECT_EQ(sent->notification.payload.imageUrl, imageUrl());
    }
    // User defined.
    {
        const auto sent = testEvent(EventType::userDefinedEvent, {}, {}, "Special Event", "", noOptions);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com","b@xxx.com","c@xxx.com"}));
        EXPECT_EQ(sent->systemId, systemId);
        EXPECT_EQ(sent->notification.title, "Special Event");
        EXPECT_EQ(sent->notification.body, "");
        EXPECT_EQ(sent->notification.payload.url, openUrl());
        EXPECT_EQ(sent->notification.payload.imageUrl, imageUrl());
    }
    // User defined with camera ref.
    {
        const auto sent = testEvent(EventType::userDefinedEvent, cameraId, {}, "Special Event", "", cameraRef);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com","b@xxx.com","c@xxx.com"}));
        EXPECT_EQ(sent->systemId, systemId);
        EXPECT_EQ(sent->notification.title, "Special Event");
        EXPECT_EQ(sent->notification.body, "");
        EXPECT_EQ(sent->notification.payload.url, openUrl(cameraId));
        EXPECT_EQ(sent->notification.payload.imageUrl, imageUrl(cameraId));
    }
}

TEST_F(PushManagerTest, MessageBody)
{
    // Default.
    {
        const auto sent = testEvent(EventType::serverStartEvent, serverId);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com","b@xxx.com","c@xxx.com"}));
        EXPECT_EQ(sent->systemId, systemId);
        EXPECT_EQ(sent->notification.title, "Server Started");
        EXPECT_EQ(sent->notification.body, "[My Cool Server]");
        EXPECT_EQ(sent->notification.payload.url, openUrl());
        EXPECT_EQ(sent->notification.payload.imageUrl, imageUrl());
    }
    // User defined.
    {
        const auto sent = testEvent(EventType::serverStartEvent, serverId, {}, {}, "... and it is named");
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com","b@xxx.com","c@xxx.com"}));
        EXPECT_EQ(sent->systemId, systemId);
        EXPECT_EQ(sent->notification.title, "Server Started");
        EXPECT_EQ(sent->notification.body, "... and it is named\n[My Cool Server]");
        EXPECT_EQ(sent->notification.payload.url, openUrl());
        EXPECT_EQ(sent->notification.payload.imageUrl, imageUrl());
    }
}

TEST_F(PushManagerTest, AddSource)
{
    // With default message body.
    {
        const auto sent = testEvent(EventType::cameraMotionEvent, cameraId, {}, {}, {}, noOptions);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com","b@xxx.com","c@xxx.com"}));
        EXPECT_EQ(sent->systemId, systemId);
        EXPECT_EQ(sent->notification.title, "Motion on Camera");
        EXPECT_EQ(sent->notification.body, "");
        EXPECT_EQ(sent->notification.payload.url, openUrl(cameraId));
        EXPECT_EQ(sent->notification.payload.imageUrl, imageUrl(cameraId));
    }
    // With user defined message body.
    {
        const auto sent = testEvent(EventType::cameraMotionEvent, cameraId, {}, {}, "WTF!", noOptions);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com","b@xxx.com","c@xxx.com"}));
        EXPECT_EQ(sent->systemId, systemId);
        EXPECT_EQ(sent->notification.title, "Motion on Camera");
        EXPECT_EQ(sent->notification.body, "WTF!");
        EXPECT_EQ(sent->notification.payload.url, openUrl(cameraId));
        EXPECT_EQ(sent->notification.payload.imageUrl, imageUrl(cameraId));
    }
}

TEST_F(PushManagerTest, UserList)
{
    // Also checking icon with default caption.
    {
        const auto sent = testEvent(EventType::networkIssueEvent, cameraId, "ab");
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com","b@xxx.com"}));
        EXPECT_EQ(sent->systemId, systemId);
        EXPECT_EQ(sent->notification.title, QString(QChar(0x26A0)) + " Network Issue");
        EXPECT_EQ(sent->notification.body, "[My Cool Camera]");
        EXPECT_EQ(sent->notification.payload.url, openUrl(cameraId));
        EXPECT_EQ(sent->notification.payload.imageUrl, imageUrl(cameraId));
    }
}

TEST_F(PushManagerTest, CaptionIcon)
{
    // Check icon with user defined caption.
    {
        const auto sent = testEvent(EventType::networkIssueEvent, cameraId, {}, "Alarm!");
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com","b@xxx.com","c@xxx.com"}));
        EXPECT_EQ(sent->systemId, systemId);
        EXPECT_EQ(sent->notification.title, QString(QChar(0x26A0)) + " Alarm!");
        EXPECT_EQ(sent->notification.body, "[My Cool Camera]");
        EXPECT_EQ(sent->notification.payload.url, openUrl(cameraId));
        EXPECT_EQ(sent->notification.payload.imageUrl, imageUrl(cameraId));
    }
}

TEST_F(PushManagerTest, Permissions)
{
    setUserPermissions("ab", vms::api::GlobalPermission::none);
    {
        const auto sent = testEvent(EventType::cameraMotionEvent, cameraId, {}, {}, {}, addSource);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"c@xxx.com"}));
        EXPECT_EQ(sent->notification.title, "Motion on Camera");
    }
    {
        const auto sent = testEvent(EventType::userDefinedEvent, cameraId, {}, {}, {}, cameraRef);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"c@xxx.com"}));
        EXPECT_EQ(sent->notification.title, "Generic Event");
    }

    setUserPermissions("bc", vms::api::GlobalPermission::viewerPermissions);
    {
        const auto sent = testEvent(EventType::cameraMotionEvent, cameraId, {}, {}, {}, addSource);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"b@xxx.com", "c@xxx.com"}));
        EXPECT_EQ(sent->notification.title, "Motion on Camera");
    }
    {
        const auto sent = testEvent(EventType::userDefinedEvent, cameraId, {}, {}, {}, cameraRef);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"b@xxx.com", "c@xxx.com"}));
        EXPECT_EQ(sent->notification.title, "Generic Event");
    }

    setUserPermissions("abc", vms::api::GlobalPermission::adminPermissions);
    {
        const auto sent = testEvent(EventType::cameraMotionEvent, cameraId, {}, {}, {}, addSource);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com", "b@xxx.com", "c@xxx.com"}));
        EXPECT_EQ(sent->notification.title, "Motion on Camera");
    }
    {
        const auto sent = testEvent(EventType::userDefinedEvent, cameraId, {}, {}, {}, cameraRef);
        ASSERT_TRUE(sent);
        EXPECT_LIST(sent->targets, ({"a@xxx.com", "b@xxx.com", "c@xxx.com"}));
        EXPECT_EQ(sent->notification.title, "Generic Event");
    }
}

TEST_F(PushManagerTest, Errors)
{
    EXPECT_TRUE(testEvent(EventType::userDefinedEvent));
    EXPECT_TRUE(testEvent(EventType::networkIssueEvent, cameraId));

    setCloudState(http::StatusCode::serviceUnavailable);
    EXPECT_FALSE(testEvent(EventType::userDefinedEvent));
    EXPECT_FALSE(testEvent(EventType::networkIssueEvent, cameraId));

    setCloudState(http::StatusCode::created);
    EXPECT_TRUE(testEvent(EventType::userDefinedEvent));
    EXPECT_TRUE(testEvent(EventType::networkIssueEvent, cameraId));
}

class PushManagerRetryTest: public PushManagerTest
{
public:
    PushManagerRetryTest():
        PushManagerTest(RetryPolicy(1, kQueueTimeout * 2, 1, kQueueTimeout * 2, 0))
    {
    }
};

TEST_F(PushManagerRetryTest, Retransmissions)
{
    generateEvent(EventType::serverStartEvent, serverId);
    ASSERT_TRUE(getRequestFromServer()) << "Normal operation";

    setCloudState(http::StatusCode::serviceUnavailable);
    generateEvent(EventType::serverStartEvent, serverId);
    ASSERT_FALSE(getRequestFromServer()) << "Server error";

    setCloudState(http::StatusCode::created);
    std::this_thread::sleep_for(kQueueTimeout * 2);
    ASSERT_TRUE(getRequestFromServer()) << "Retransmission";

    setCloudState(http::StatusCode::serviceUnavailable);
    generateEvent(EventType::serverStartEvent, serverId);
    ASSERT_FALSE(getRequestFromServer()) << "Server error";

    std::this_thread::sleep_for(kQueueTimeout);
    ASSERT_FALSE(getRequestFromServer()) << "Server error on retransmission";

    setCloudState(http::StatusCode::created);
    generateEvent(EventType::serverStartEvent, serverId);
    ASSERT_TRUE(getRequestFromServer()) << "Normal operation";
}

} // namespace nx::vms::server::event::test
