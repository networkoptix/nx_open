// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/server_runtime_event_data.h>

class QnCommonMessageProcessor;

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ServerRuntimeEventConnector: public QObject
{
    Q_OBJECT

public:
    void setMessageProcessor(QnCommonMessageProcessor* messageProcessor);

signals:
    void deviceAgentSettingsMaybeChanged(QnUuid deviceId, QnUuid engineId);
    void deviceFootageChanged(const std::vector<QnUuid>& deviceIds);
    void analyticsStorageParametersChanged(QnUuid serverId);
    void deviceAdvancedSettingsManifestChanged(const std::set<QnUuid>& deviceIds);

private:
    void at_serverRuntimeEventOccurred(const nx::vms::api::ServerRuntimeEventData& eventData);
};

} // namespace nx::vms::client::core
