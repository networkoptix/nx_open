#pragma once

#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::event {

class ServerRuntimeEventManager: public ServerModuleAware
{
public:
    ServerRuntimeEventManager(QnMediaServerModule* serverModule);

    void triggerDeviceAgentSettingsMaybeChangedEvent(QnUuid deviceId, QnUuid engineId);
};

} // namespace nx::vms::server::event
