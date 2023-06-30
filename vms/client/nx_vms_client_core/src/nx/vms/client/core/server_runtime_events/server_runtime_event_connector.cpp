// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_runtime_event_connector.h"

#include <functional>

#include <api/common_message_processor.h>
#include <nx/fusion/serialization/json.h>

namespace nx::vms::client::core {

using namespace nx::vms::api;

void ServerRuntimeEventConnector::setMessageProcessor(QnCommonMessageProcessor* messageProcessor)
{
    if (messageProcessor)
    {
        connect(
            messageProcessor,
            &QnCommonMessageProcessor::serverRuntimeEventOccurred,
            this,
            &ServerRuntimeEventConnector::at_serverRuntimeEventOccurred);
    }
}

void ServerRuntimeEventConnector::at_serverRuntimeEventOccurred(
    const ServerRuntimeEventData& eventData)
{
    static const nx::utils::log::Tag kLogTag(typeid(*this));

    switch (eventData.eventType)
    {
        case ServerRuntimeEventType::undefined:
        {
            break;
        }
        case ServerRuntimeEventType::deviceAgentSettingsMaybeChanged:
        {
            DeviceAgentSettingsMaybeChangedData result;
            if (!NX_ASSERT(QJson::deserialize(eventData.eventData, &result)))
                return;

            emit deviceAgentSettingsMaybeChanged(
                result.deviceId,
                result.engineId);

            return;
        }
        case ServerRuntimeEventType::deviceFootageChanged:
        {
            DeviceFootageChangedData result;
            if (!NX_ASSERT(QJson::deserialize(eventData.eventData, &result)))
                return;

            emit deviceFootageChanged(result.deviceIds);

            return;
        }
        case ServerRuntimeEventType::analyticsStorageParametersChanged:
        {
            AnalyticsStorageParametersChangedData result;
            if (!NX_ASSERT(QJson::deserialize(eventData.eventData, &result)))
                return;

            emit analyticsStorageParametersChanged(result.serverId);

            return;
        }
        case ServerRuntimeEventType::deviceAdvancedSettingsManifestChanged:
        {
            DeviceAdvancedSettingsManifestChangedData result;
            if (!NX_ASSERT(QJson::deserialize(eventData.eventData, &result)))
                return;

            emit deviceAdvancedSettingsManifestChanged(result.deviceIds);

            return;
        }
    }
    NX_WARNING(this, "Unexpected event type %1", eventData.eventType);
}

} // namespace nx::vms::client::core
