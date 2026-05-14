// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_manager_factory.h"

#include <QtCore/QObject>

#include <api/server_rest_connection.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/server_runtime_event_data.h>
#include <nx/vms/client/core/server_runtime_events/server_runtime_event_connector.h>
#include <nx/vms/client/core/system_context.h>

#include "analytics_settings_manager.h"

namespace nx::vms::client::core {

using namespace nx::vms::api;

namespace {

class AnalyticsSettingsServerRestApiInterface:
    public AnalyticsSettingsServerInterface,
    public SystemContextAware
{
public:
    AnalyticsSettingsServerRestApiInterface(SystemContext* systemContext, QObject* owner):
        SystemContextAware(systemContext),
        m_owner(owner)
    {
    }

    virtual rest::Handle getSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback) override
    {
        if (!NX_ASSERT(device) || !NX_ASSERT(engine) || !NX_ASSERT(m_owner))
            return {};

        if (const auto api = connectedServerApi())
        {
            return api->getDeviceAnalyticsSettings(
                device,
                engine,
                std::move(callback),
                m_owner.get());
        }

        return {};
    }

    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settingsValues,
        const QJsonObject& settingsModel,
        AnalyticsSettingsCallback callback) override
    {
        if (!NX_ASSERT(device) || !NX_ASSERT(engine) || !NX_ASSERT(m_owner))
            return {};

        if (const auto api = connectedServerApi())
        {
            return api->setDeviceAnalyticsSettings(
                device,
                engine,
                settingsValues,
                settingsModel,
                std::move(callback),
                m_owner.get());
        }

        return {};
    }

    virtual rest::Handle activeSettingsChanged(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QString& activeElement,
        const QJsonObject& settingsModel,
        const QJsonObject& settingsValues,
        const QJsonObject& paramValues,
        AnalyticsActiveSettingsCallback callback) override
    {
        if (!NX_ASSERT(device) || !NX_ASSERT(engine) || !NX_ASSERT(m_owner))
            return {};

        if (const auto api = connectedServerApi())
        {
            return api->deviceAnalyticsActiveSettingsChanged(
                device,
                engine,
                activeElement,
                settingsModel,
                settingsValues,
                paramValues,
                std::move(callback),
                m_owner.get());
        }

        return {};
    }

private:
    QPointer<QObject> m_owner;
};

} // namespace

std::unique_ptr<AnalyticsSettingsManager>
AnalyticsSettingsManagerFactory::createAnalyticsSettingsManager(SystemContext* systemContext)
{
    auto manager = std::make_unique<AnalyticsSettingsManager>(systemContext);

    manager->setServerInterface(std::make_unique<AnalyticsSettingsServerRestApiInterface>(
        systemContext, manager.get()));

    QObject::connect(
        systemContext->serverRuntimeEventConnector(),
        &ServerRuntimeEventConnector::deviceAgentSettingsMaybeChanged,
        manager.get(),
        [managerPtr = manager.get()](const DeviceAgentSettingsMaybeChangedData& payload)
        {
            managerPtr->storeSettings({payload.deviceId, payload.engineId}, payload.settingsData);
        });

    return manager;
}

} // namespace nx::vms::client::core
