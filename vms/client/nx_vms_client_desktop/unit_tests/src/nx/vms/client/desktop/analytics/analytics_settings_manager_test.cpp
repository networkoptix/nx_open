#include "analytics_settings_manager_test.h"

#include <common/static_common_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/core/access/access_types.h>

#include <nx/vms/api/analytics/device_analytics_settings_data.h>

namespace nx::vms::client::desktop {
namespace test {

using namespace nx::vms::api;
using namespace nx::vms::api::analytics; //< TODO: Remove lexical reduplication.
using namespace nx::vms::common;

AnalyticsSettingsMockApiInterface::AnalyticsSettingsMockApiInterface()
{
}

rest::Handle AnalyticsSettingsMockApiInterface::getSettings(
    const QnVirtualCameraResourcePtr& device,
    const AnalyticsEngineResourcePtr& engine,
    AnalyticsSettingsCallback callback)
{
    return makeRequest(RequestInfo::Type::get, device, engine, callback).handle;
}

rest::Handle AnalyticsSettingsMockApiInterface::applySettings(
    const QnVirtualCameraResourcePtr& device,
    const AnalyticsEngineResourcePtr& engine,
    const QJsonObject& settings,
    AnalyticsSettingsCallback callback)
{
    return makeRequest(RequestInfo::Type::apply, device, engine, callback).handle;
}

bool AnalyticsSettingsMockApiInterface::requestWasSent(const DeviceAgentId& agentId) const
{
    return std::find_if(m_requests.cbegin(), m_requests.cend(),
        [&agentId](const auto& info) { return info.agentId == agentId; }) != m_requests.cend();
}

void AnalyticsSettingsMockApiInterface::sendReply(
    const DeviceAgentId& agentId,
    const DeviceAnalyticsSettingsResponse& response,
    bool success)
{
    auto request = std::find_if(m_requests.begin(), m_requests.end(),
        [&agentId](const auto& info) { return info.agentId == agentId; });
    NX_ASSERT(request != m_requests.end());
    request->callback(success, request->handle, response);
    m_requests.erase(request);
}

AnalyticsSettingsMockApiInterface::RequestInfo AnalyticsSettingsMockApiInterface::makeRequest(
    RequestInfo::Type type,
    const QnVirtualCameraResourcePtr& device,
    const AnalyticsEngineResourcePtr& engine,
    AnalyticsSettingsCallback callback)
{
    m_lastHandle++;

    m_requests.push_back({
        type,
        m_lastHandle,
        { device->getId(), engine->getId() },
        callback }
    );
    return m_requests.back();
}

ListenerNotifier::ListenerNotifier(const AnalyticsSettingsListenerPtr& listener)
{
    connect(listener.get(), &AnalyticsSettingsListener::dataChanged, this,
        [this](const DeviceAgentData& data)
        {
            lastData = data;
            ++counter;
        });
}


// virtual void SetUp() will be called before each test is run.

struct AnalyticsSettingsManagerTest::Environment
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

AnalyticsSettingsManagerTest::AnalyticsSettingsManagerTest()
{
}

AnalyticsSettingsManagerTest::~AnalyticsSettingsManagerTest()
{
}

void AnalyticsSettingsManagerTest::SetUp()
{
    m_environment.reset(new Environment());
    m_serverInterfaceMock = std::make_shared<AnalyticsSettingsMockApiInterface>();
    m_manager.reset(new AnalyticsSettingsManager());
    m_manager->setResourcePool(resourcePool());
    m_manager->setServerInterface(m_serverInterfaceMock);
}

// virtual void TearDown() will be called after each test is run.

void AnalyticsSettingsManagerTest::TearDown()
{
    m_manager.reset();
    m_serverInterfaceMock.reset();
    m_environment.reset();
}

nx::CameraResourceStubPtr AnalyticsSettingsManagerTest::addCamera()
{
    nx::CameraResourceStubPtr camera(new nx::CameraResourceStub());
    resourcePool()->addResource(camera);
    return camera;
}

AnalyticsEngineResourcePtr AnalyticsSettingsManagerTest::addEngine()
{
    AnalyticsEngineResourcePtr engine(new AnalyticsEngineResource());
    engine->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(engine);
    return engine;
}

DeviceAgentId AnalyticsSettingsManagerTest::makeDeviceAgent()
{
    auto camera = addCamera();
    auto engine = addEngine();
    return DeviceAgentId{ camera->getId(), engine->getId() };
}

AnalyticsSettingsManager* AnalyticsSettingsManagerTest::manager() const
{
    return m_manager.data();
}

QnResourcePool* AnalyticsSettingsManagerTest::resourcePool() const
{
    return m_environment->commonModule.resourcePool();
}

} // namespace test
} // namespace nx::vms::client::desktop
