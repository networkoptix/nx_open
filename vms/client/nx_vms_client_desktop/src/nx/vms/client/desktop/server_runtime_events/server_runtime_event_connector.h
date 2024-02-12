// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <nx/utils/uuid.h>
#include <nx/vms/api/data/server_runtime_event_data.h>

class QnCommonMessageProcessor;

namespace nx::vms::client::desktop {

class ServerRuntimeEventConnector: public QObject
{
    Q_OBJECT

public:
    void setMessageProcessor(QnCommonMessageProcessor* messageProcessor);

signals:
    void deviceAgentSettingsMaybeChanged(nx::Uuid deviceId, nx::Uuid engineId);
    void deviceFootageChanged(const std::vector<nx::Uuid>& deviceIds);
    void analyticsStorageParametersChanged(nx::Uuid serverId);
    void deviceAdvancedSettingsManifestChanged(const std::set<nx::Uuid>& deviceIds);

private:
    void at_serverRuntimeEventOccurred(const nx::vms::api::ServerRuntimeEventData& eventData);
};

} // namespace nx::vms::client::desktop
