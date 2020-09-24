#include "analytics_settings_test_fixture.h"

#include <common/static_common_module.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/core/access/access_types.h>

#include <nx/vms/api/analytics/device_agent_settings_response.h>

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
    const QJsonObject& /*settings*/,
    const QnUuid& /*settingsModelId*/,
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
    const DeviceAgentSettingsResponse& response,
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

struct AnalyticsSettingsTestFixture::Environment
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

AnalyticsSettingsTestFixture::AnalyticsSettingsTestFixture()
{
}

AnalyticsSettingsTestFixture::~AnalyticsSettingsTestFixture()
{
}

void AnalyticsSettingsTestFixture::SetUp()
{
    m_environment.reset(new Environment());
    m_serverInterfaceMock = std::make_shared<AnalyticsSettingsMockApiInterface>();
    m_manager.reset(new AnalyticsSettingsManager());
    m_manager->setResourcePool(resourcePool());
    m_manager->setServerInterface(m_serverInterfaceMock);
}

void AnalyticsSettingsTestFixture::TearDown()
{
    m_manager.reset();
    m_serverInterfaceMock.reset();
    m_environment.reset();
}

nx::CameraResourceStubPtr AnalyticsSettingsTestFixture::addCamera()
{
    nx::CameraResourceStubPtr camera(new nx::CameraResourceStub());
    resourcePool()->addResource(camera);
    return camera;
}

AnalyticsEngineResourcePtr AnalyticsSettingsTestFixture::addEngine()
{
    AnalyticsEngineResourcePtr engine(new AnalyticsEngineResource());
    engine->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(engine);
    return engine;
}

AnalyticsSettingsManager* AnalyticsSettingsTestFixture::manager() const
{
    return m_manager.data();
}

QnResourcePool* AnalyticsSettingsTestFixture::resourcePool() const
{
    return m_environment->commonModule.resourcePool();
}

} // namespace test
} // namespace nx::vms::client::desktop
