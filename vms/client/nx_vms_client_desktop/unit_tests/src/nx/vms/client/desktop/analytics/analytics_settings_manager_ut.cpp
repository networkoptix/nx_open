#include <gtest/gtest.h>

#include <deque>

#include <common/static_common_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/core/access/access_types.h>

#include <test_support/resource/camera_resource_stub.h>

#include <nx/vms/client/desktop/analytics/analytics_settings_manager.h>

#include <nx/vms/api/analytics/device_analytics_settings_data.h>

#include <nx/fusion/model_functions.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::vms::api;
using namespace nx::vms::api::analytics; //< TODO: Remove lexical reduplication.
using namespace nx::vms::common;

class AnalyticsSettingsMockApiInterface: public AnalyticsSettingsServerInterface
{
public:
    struct RequestInfo
    {
        enum class Type { get, apply };
        Type type = Type::get;
        rest::Handle handle = {};
        DeviceAgentId agentId;
        AnalyticsSettingsCallback callback;
    };

    AnalyticsSettingsMockApiInterface(){}

    virtual rest::Handle getSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback) override
    {
        return makeRequest(RequestInfo::Type::get, device, engine, callback).handle;
    }

    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        AnalyticsSettingsCallback callback) override
    {
        return makeRequest(RequestInfo::Type::apply, device, engine, callback).handle;
    }

    bool requestWasSent(const DeviceAgentId& agentId) const
    {
        return std::find_if(m_requests.cbegin(), m_requests.cend(),
            [&agentId](const auto& info) { return info.agentId == agentId; } ) != m_requests.cend();
    }

    void sendReply(
        const DeviceAgentId& agentId,
        const DeviceAnalyticsSettingsResponse& response,
        bool success = true)
    {
        auto request = std::find_if(m_requests.begin(), m_requests.end(),
            [&agentId](const auto& info) { return info.agentId == agentId; } );
        NX_ASSERT(request != m_requests.end());
        request->callback(success, request->handle, response);
        m_requests.erase(request);
    }

private:
    RequestInfo makeRequest(
        RequestInfo::Type type,
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback)
    {
        m_lastHandle++;

        m_requests.push_back({
            type,
            m_lastHandle,
            {device->getId(), engine->getId()},
            callback}
        );
        return m_requests.back();
    }

    rest::Handle m_lastHandle = 0;
    std::deque<RequestInfo> m_requests;
};
using AnalyticsSettingsMockApiInterfacePtr = std::shared_ptr<AnalyticsSettingsMockApiInterface>;

/**
 * Class for convinient checks if listener dataChanged signal was emitted.
 */
class ListenerNotifier: public QObject
{
public:
    ListenerNotifier(const AnalyticsSettingsListenerPtr& listener)
    {
        connect(listener.get(), &AnalyticsSettingsListener::dataChanged, this,
            [this](const DeviceAgentData& data)
            {
                lastData = data;
                ++counter;
            });
    }

    DeviceAgentData lastData;
    int counter = 0;
};

class AnalyticsSettingsManagerTest: public ::testing::Test
{
protected:
    // virtual void SetUp() will be called before each test is run.
    virtual void SetUp()
    {
        m_environment.reset(new Environment());
        m_serverInterfaceMock = std::make_shared<AnalyticsSettingsMockApiInterface>();
        m_manager.reset(new AnalyticsSettingsManager());
        m_manager->setResourcePool(resourcePool());
        m_manager->setServerInterface(m_serverInterfaceMock);
    }

    // virtual void TearDown() will be called after each test is run.
    virtual void TearDown()
    {
        m_manager.reset();
        m_serverInterfaceMock.reset();
        m_environment.reset();
    }

    nx::CameraResourceStubPtr addCamera()
    {
        nx::CameraResourceStubPtr camera(new nx::CameraResourceStub());
        resourcePool()->addResource(camera);
        return camera;
    }

    AnalyticsEngineResourcePtr addEngine()
    {
        AnalyticsEngineResourcePtr engine(new AnalyticsEngineResource());
        engine->setIdUnsafe(QnUuid::createUuid());
        resourcePool()->addResource(engine);
        return engine;
    }

    DeviceAgentId makeDeviceAgent()
    {
        auto camera = addCamera();
        auto engine = addEngine();
        return DeviceAgentId{camera->getId(), engine->getId()};
    }

    AnalyticsSettingsManager* manager() const
    {
        return m_manager.data();
    }

    QnResourcePool* resourcePool() const
    {
        return m_environment->commonModule.resourcePool();
    }

    struct Environment
    {
        Environment():
            staticCommonModule(),
            commonModule(
                /*clientMode*/ false,
                /*resourceAccessMode*/ nx::core::access::Mode::direct)
        {
        }

        QnStaticCommonModule staticCommonModule;
        QnCommonModule commonModule;
    };

    // Declares the variables your tests want to use.

    QScopedPointer<Environment> m_environment;
    QScopedPointer<AnalyticsSettingsManager> m_manager;
    AnalyticsSettingsMockApiInterfacePtr m_serverInterfaceMock;
};

static const auto kEmptyReply = QJson::deserialized<DeviceAnalyticsSettingsResponse>(R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {},
        "settingsValues": {}
    }
)json");

static const auto kData1Reply = QJson::deserialized<DeviceAnalyticsSettingsResponse>(R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {
            "items": [ { "name": "generateCars", "type": "CheckBox" } ]
            "type": "Settings"
        },
        "settingsValues": { "generateCars": true }
    }
)json");

static const auto kData2Reply= QJson::deserialized<DeviceAnalyticsSettingsResponse>(R"json(
    {
        "analyzedStreamIndex": "primary",
        "settingsModel": {
            "items": [ { "name": "generatePersons", "type": "CheckBox" } ]
            "type": "Settings"
        },
        "settingsValues": { "generatePersons": false }
    }
)json");

/**
 * Validate VMS-17327: camera settings dialog did not get an update when a loaded model was empty.
 * Solution: add request status to a passed data, so dialog will know the status for sure.
 */
TEST_F(AnalyticsSettingsManagerTest, statusUpdateOnEmptyReply)
{
    const auto agentId = makeDeviceAgent();
    auto listener = manager()->getListener(agentId);
    ListenerNotifier notifier(listener);

    // Check manager made a request here.
    ASSERT_TRUE(m_serverInterfaceMock->requestWasSent(agentId));
    // Check listener is still waiting.
    ASSERT_TRUE(listener->data().status == DeviceAgentData::Status::loading);

    // Emulate network response.
    const auto reply = kEmptyReply;
    m_serverInterfaceMock->sendReply(agentId, reply);

    // Check listener sent actual data notification.
    ASSERT_EQ(notifier.counter, 1);
    ASSERT_TRUE(listener->data().status == DeviceAgentData::Status::ok);
}

TEST_F(AnalyticsSettingsManagerTest, listenToExternalChanges)
{
    const auto agentId = makeDeviceAgent();
    auto listener = manager()->getListener(agentId);
    ListenerNotifier notifier(listener);
    m_serverInterfaceMock->sendReply(agentId, kData1Reply);

    ASSERT_FALSE(m_serverInterfaceMock->requestWasSent(agentId));
    manager()->refreshSettings(agentId);
    ASSERT_TRUE(m_serverInterfaceMock->requestWasSent(agentId));

    // Emulate network response.
    m_serverInterfaceMock->sendReply(agentId, kData2Reply);

    // Check consumer received actual data.
    ASSERT_EQ(notifier.lastData.model, kData2Reply.settingsModel);
    ASSERT_EQ(notifier.lastData.values, kData2Reply.settingsValues);
}

} // namespace test
} // namespace nx::vms::client::desktop
