// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_test_fixture.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/core/access/access_types.h>
#include <nx/vms/api/analytics/device_agent_settings_response.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/system_context.h>
#include <nx/vms/common/test_support/api/message_processor_mock.h>

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
    core::AnalyticsSettingsCallback callback)
{
    return makeRequest(RequestInfo::Type::get, device, engine, callback).handle;
}

rest::Handle AnalyticsSettingsMockApiInterface::applySettings(
    const QnVirtualCameraResourcePtr& device,
    const AnalyticsEngineResourcePtr& engine,
    const QJsonObject& /*settings*/,
    const QnUuid& /*settingsModelId*/,
    core::AnalyticsSettingsCallback callback)
{
    return makeRequest(RequestInfo::Type::apply, device, engine, callback).handle;
}

rest::Handle AnalyticsSettingsMockApiInterface::activeSettingsChanged(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QString& activeElement,
    const QJsonObject& settingsModel,
    const QJsonObject& settingsValues,
    const QJsonObject& paramValues,
    core::AnalyticsActiveSettingsCallback callback)
{
    return makeRequest(RequestInfo::Type::get, device, engine, callback).handle;
}

bool AnalyticsSettingsMockApiInterface::requestWasSent(const core::DeviceAgentId& agentId) const
{
    return std::find_if(m_requests.cbegin(), m_requests.cend(),
        [&agentId](const auto& info) { return info.agentId == agentId; }) != m_requests.cend();
}

void AnalyticsSettingsMockApiInterface::sendReply(
    const core::DeviceAgentId& agentId,
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
    core::AnalyticsSettingsCallback callback)
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

AnalyticsSettingsMockApiInterface::RequestInfo AnalyticsSettingsMockApiInterface::makeRequest(
    RequestInfo::Type type,
    const QnVirtualCameraResourcePtr& device,
    const AnalyticsEngineResourcePtr& engine,
    core::AnalyticsActiveSettingsCallback callback)
{
    return {};
}

ListenerNotifier::ListenerNotifier(const core::AnalyticsSettingsListenerPtr& listener)
{
    connect(listener.get(), &core::AnalyticsSettingsListener::dataChanged, this,
        [this](const core::DeviceAgentData& data)
        {
            lastData = data;
            ++counter;
        });
}

void AnalyticsSettingsTestFixture::SetUp()
{
    m_serverInterfaceMock = std::make_shared<AnalyticsSettingsMockApiInterface>();
    createMessageProcessor();
    m_manager.reset(new core::AnalyticsSettingsManager(systemContext()));
    m_manager->setServerInterface(m_serverInterfaceMock);
}

void AnalyticsSettingsTestFixture::TearDown()
{
    m_manager.reset();
    m_serverInterfaceMock.reset();
}

AnalyticsEngineResourcePtr AnalyticsSettingsTestFixture::addEngine()
{
    AnalyticsEngineResourcePtr engine(new AnalyticsEngineResource());
    engine->setIdUnsafe(QnUuid::createUuid());
    resourcePool()->addResource(engine);
    return engine;
}

core::AnalyticsSettingsManager* AnalyticsSettingsTestFixture::manager() const
{
    return m_manager.data();
}

QnResourcePool* AnalyticsSettingsTestFixture::resourcePool() const
{
    return systemContext()->resourcePool();
}

} // namespace test
} // namespace nx::vms::client::desktop
