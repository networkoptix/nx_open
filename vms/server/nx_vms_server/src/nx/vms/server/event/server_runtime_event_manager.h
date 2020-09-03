#pragma once

#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/api/data/server_runtime_event_data.h>

namespace nx::vms::server::event {

class ServerRuntimeEventManager: public ServerModuleAware
{

public:
    ServerRuntimeEventManager(QnMediaServerModule* serverModule);

    void triggerDeviceAgentSettingsMaybeChangedEvent(
        QnUuid deviceId,
        QnUuid engineId,
        nx::vms::api::SettingsData settingsData);

    void triggerDeviceFootageChangedEvent(std::vector<QnUuid> deviceIds);

    void triggerAnalyticsStorageParametersChanged(QnUuid serverId);

private:
    void sendEvent(const nx::vms::api::ServerRuntimeEventData& eventData);
};

} // namespace nx::vms::server::event
