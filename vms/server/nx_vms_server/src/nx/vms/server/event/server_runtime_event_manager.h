#pragma once

#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/api/data/server_runtime_event_data.h>

namespace nx::vms::server::event {

class ServerRuntimeEventManager: public ServerModuleAware
{
public:
    ServerRuntimeEventManager(QnMediaServerModule* serverModule);

    void triggerDeviceAgentSettingsMaybeChangedEvent(QnUuid deviceId, QnUuid engineId);

    void triggerDeviceFootageChangedEvent(std::vector<QnUuid> deviceIds);

private:
    void sendEvent(const nx::vms::api::ServerRuntimeEventData& eventData);
};

} // namespace nx::vms::server::event
