// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_test_fixture.h"

#include <core/resource/camera_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/api/analytics/device_agent_settings_response.h>
#include <nx/vms/common/api/message_processor_mock.h>
#include <nx/vms/common/resource/analytics_engine_resource.h>
#include <nx/vms/common/system_context.h>

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
    return makeRequest(RequestInfo::Type::get, device, engine, std::move(callback));
}

rest::Handle AnalyticsSettingsMockApiInterface::applySettings(
    const QnVirtualCameraResourcePtr& device,
    const AnalyticsEngineResourcePtr& engine,
    const QJsonObject& /*settingsValues*/,
    const QJsonObject& /*settingsModel*/,
    core::AnalyticsSettingsCallback callback)
{
    return makeRequest(RequestInfo::Type::apply, device, engine, std::move(callback));
}

rest::Handle AnalyticsSettingsMockApiInterface::activeSettingsChanged(
    const QnVirtualCameraResourcePtr& device,
    const nx::vms::common::AnalyticsEngineResourcePtr& engine,
    const QString& /*activeElement*/,
    const QJsonObject& /*settingsModel*/,
    const QJsonObject& /*settingsValues*/,
    const QJsonObject& /*paramValues*/,
    core::AnalyticsActiveSettingsCallback callback)
{
    return makeRequest(RequestInfo::Type::get, device, engine, std::move(callback));
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

rest::Handle AnalyticsSettingsMockApiInterface::makeRequest(
    RequestInfo::Type type,
    const QnVirtualCameraResourcePtr& device,
    const AnalyticsEngineResourcePtr& engine,
    core::AnalyticsSettingsCallback callback)
{
    m_lastHandle++;

    m_requests.push_back({
        .type = type,
        .handle = m_lastHandle,
        .agentId = {device->getId(), engine->getId()},
        .callback = std::move(callback)
    });

    return m_requests.back().handle;
}

rest::Handle AnalyticsSettingsMockApiInterface::makeRequest(
    RequestInfo::Type /*type*/,
    const QnVirtualCameraResourcePtr& /*device*/,
    const AnalyticsEngineResourcePtr& /*engine*/,
    core::AnalyticsActiveSettingsCallback /*callback*/)
{
    return {};
}

ListenerNotifier::ListenerNotifier(const core::AnalyticsSettingsListenerPtr& listener)
{
    connect(listener.get(), &core::AnalyticsSettingsListener::dataChanged, this,
        [this](const core::DeviceAgentData& data)
        {
            lastData = data;
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
    engine->setIdUnsafe(nx::Uuid::createUuid());
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
