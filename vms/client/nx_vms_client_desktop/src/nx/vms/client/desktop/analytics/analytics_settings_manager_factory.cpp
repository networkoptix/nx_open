#include "analytics_settings_manager_factory.h"
#include "analytics_settings_manager.h"

#include <QtCore/QObject>

#include <api/server_rest_connection.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <client/desktop_client_message_processor.h>

#include <nx/utils/guarded_callback.h>

namespace nx::vms::client::desktop {

using namespace nx::vms::api;

namespace {

class AnalyticsSettingsServerRestApiInterface: public AnalyticsSettingsServerInterface
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
        const auto server = NX_ASSERT(device)
            ? device->getParentServer()
            : QnMediaServerResourcePtr();

        if (!NX_ASSERT(server) || !NX_ASSERT(engine) || !NX_ASSERT(m_owner))
            return {};

        return server->restConnection()->getDeviceAnalyticsSettings(
            device,
            engine,
            nx::utils::guarded(m_owner, callback),
            m_owner->thread());
    }

    virtual rest::Handle applySettings(
        const QnVirtualCameraResourcePtr& device,
        const nx::vms::common::AnalyticsEngineResourcePtr& engine,
        const QJsonObject& settings,
        AnalyticsSettingsCallback callback) override
    {
        const auto server = NX_ASSERT(device)
            ? device->getParentServer()
            : QnMediaServerResourcePtr();

        if (!NX_ASSERT(server) || !NX_ASSERT(engine) || !NX_ASSERT(m_owner))
            return {};

        return server->restConnection()->setDeviceAnalyticsSettings(
            device,
            engine,
            settings,
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
    QnDesktopClientMessageProcessor* messageProcessor)
{
    auto manager = std::make_unique<AnalyticsSettingsManager>();
    manager->setResourcePool(resourcePool);

    auto serverInterface = std::make_shared<AnalyticsSettingsServerRestApiInterface>(manager.get());
    manager->setServerInterface(serverInterface);

    QObject::connect(
        messageProcessor,
        &QnDesktopClientMessageProcessor::serverRuntimeEventOccurred,
        manager.get(),
        [managerPtr = manager.get()](const ServerRuntimeEventData& eventData)
        {
            if (eventData.eventType == ServerRuntimeEventType::deviceAgentSettingsMaybeChanged)
            {
                const auto payload =
                    QJson::deserialized<DeviceAgentSettingsMaybeChangedData>(
                        eventData.eventData);

                managerPtr->refreshSettings({payload.deviceId, payload.engineId});
            }
        });

    return manager;
}

} // namespace nx::vms::client::desktop
