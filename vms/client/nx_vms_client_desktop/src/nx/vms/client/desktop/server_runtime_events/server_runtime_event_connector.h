#pragma once

#include <vector>

#include <nx/utils/uuid.h>

#include <utils/common/connective.h>

#include <nx/vms/api/data/server_runtime_event_data.h>

class QnDesktopClientMessageProcessor;

namespace nx::vms::client::desktop {

class ServerRuntimeEventConnector: public Connective<QObject>
{
    Q_OBJECT
public:
    ServerRuntimeEventConnector(QnDesktopClientMessageProcessor* messageProcessor);

signals:
    void deviceAgentSettingsMaybeChanged(QnUuid deviceId, QnUuid engineId);
    void deviceFootageChanged(const std::vector<QnUuid>& deviceIds);
    void analyticsStorageParametersChanged(QnUuid serverId);

private:
    void at_serverRuntimeEventOccurred(const nx::vms::api::ServerRuntimeEventData& eventData);
};

} // namespace nx::vms::client::desktop
