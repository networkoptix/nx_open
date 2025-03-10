// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <vector>

#include <nx/utils/uuid.h>

class QnCommonMessageProcessor;

namespace nx::vms::api {

enum class EventReason;

struct ServerRuntimeEventData;
struct SiteHealthMessage;

} // namespace nx::vms::api

namespace nx::vms::client::core {

class NX_VMS_CLIENT_CORE_API ServerRuntimeEventConnector: public QObject
{
    Q_OBJECT

public:
    void setMessageProcessor(QnCommonMessageProcessor* messageProcessor);

signals:
    void deviceAgentSettingsMaybeChanged(nx::Uuid deviceId, nx::Uuid engineId);
    void deviceFootageChanged(const std::vector<nx::Uuid>& deviceIds);
    void analyticsStorageParametersChanged(nx::Uuid serverId);
    void deviceAdvancedSettingsManifestChanged(const std::set<nx::Uuid>& deviceIds);
    void serviceDisabled(nx::vms::api::EventReason reason, const std::set<nx::Uuid>& deviceIds);
    void siteHealthMessage(const nx::vms::api::SiteHealthMessage& message);

private:
    void at_serverRuntimeEventOccurred(const nx::vms::api::ServerRuntimeEventData& eventData);
};

} // namespace nx::vms::client::core
