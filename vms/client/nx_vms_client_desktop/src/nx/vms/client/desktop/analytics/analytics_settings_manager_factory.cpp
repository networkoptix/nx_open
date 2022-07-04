// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "analytics_settings_manager_factory.h"

#include <QtCore/QObject>

#include <api/common_message_processor.h>
#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/api/data/server_runtime_event_data.h>
#include <nx/vms/client/core/network/remote_connection_aware.h>
#include <utils/common/delayed.h>

#include "analytics_settings_manager.h"

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

namespace {

class AnalyticsSettingsServerRestApiInterface:
    public AnalyticsSettingsServerInterface,
    public core::RemoteConnectionAware
{
public:
    AnalyticsSettingsServerRestApiInterface(QObject* owner):
        m_owner(owner)
    {}

    virtual rest::Handle getSettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        AnalyticsSettingsCallback callback) override
    {
        if (!NX_ASSERT(device) || !NX_ASSERT(engine) || !NX_ASSERT(m_owner))
            return {};

        return connectedServerApi()->getDeviceAnalyticsSettings(
            device,
            engine,
            nx::utils::guarded(m_owner, callback),
            m_owner->thread());
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

        return connectedServerApi()->setDeviceAnalyticsSettings(
            device,
            engine,
            settings,
            settingsModelId,
            nx::utils::guarded(m_owner, callback),
            m_owner->thread());
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

        return connectedServerApi()->deviceAnalyticsActiveSettingsChanged(
            device,
            engine,
            activeElement,
            settingsModel,
            settingsValues,
            paramValues,
            nx::utils::guarded(m_owner, callback),
            m_owner->thread());
    }

private:
    QPointer<QObject> m_owner;
};

} // namespace

std::unique_ptr<AnalyticsSettingsManager>
AnalyticsSettingsManagerFactory::createAnalyticsSettingsManager(
    QnResourcePool* resourcePool,
    QnCommonMessageProcessor* messageProcessor)
{
    auto manager = std::make_unique<AnalyticsSettingsManager>();
    manager->setContext(resourcePool, messageProcessor);

    auto serverInterface = std::make_shared<AnalyticsSettingsServerRestApiInterface>(manager.get());
    manager->setServerInterface(serverInterface);

    QObject::connect(
        messageProcessor,
        &QnCommonMessageProcessor::serverRuntimeEventOccurred,
        manager.get(),
        [managerPtr = manager.get()](const ServerRuntimeEventData& eventData)
        {
            if (eventData.eventType == ServerRuntimeEventType::deviceAgentSettingsMaybeChanged)
            {
                const auto payload =
                    QJson::deserialized<DeviceAgentSettingsMaybeChangedData>(
                        eventData.eventData);

                managerPtr->storeSettings(
                    {payload.deviceId, payload.engineId},
                    payload.settingsData);
            }
        });

    return manager;
}

} // namespace nx::vms::client::desktop
