// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_manager_factory.h"

#include <QtCore/QObject>

#include <api/common_message_processor.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/reflect/json.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/server_runtime_event_data.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/delayed.h>

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
                nx::utils::guarded(m_owner, callback),
                m_owner->thread());
        }

        return {};
    }

    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        const QnUuid& settingsModelId,
        AnalyticsSettingsCallback callback) override
    {
        if (!NX_ASSERT(device) || !NX_ASSERT(engine) || !NX_ASSERT(m_owner))
            return {};

        if (const auto api = connectedServerApi())
        {
            return api->setDeviceAnalyticsSettings(
                device,
                engine,
                settings,
                settingsModelId,
                nx::utils::guarded(m_owner, callback),
                m_owner->thread());
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
                nx::utils::guarded(m_owner, callback),
                m_owner->thread());
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

    auto serverInterface = std::make_shared<AnalyticsSettingsServerRestApiInterface>(
        systemContext,
        manager.get());
    manager->setServerInterface(serverInterface);

    QObject::connect(
        systemContext->messageProcessor(),
        &QnCommonMessageProcessor::serverRuntimeEventOccurred,
        manager.get(),
        [managerPtr = manager.get()](const ServerRuntimeEventData& eventData)
        {
            if (eventData.eventType == ServerRuntimeEventType::deviceAgentSettingsMaybeChanged)
            {
                const auto [payload, success] =
                    nx::reflect::json::deserialize<DeviceAgentSettingsMaybeChangedData>(
                        eventData.eventData.toStdString());

                if (!NX_ASSERT(success, "Could not deserialize event data"))
                    return;

                managerPtr->storeSettings(
                    {payload.deviceId, payload.engineId},
                    payload.settingsData);
            }
        });

    return manager;
}

} // namespace nx::vms::client::core
