// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_runtime_event_connector.h"

#include <functional>

#include <api/common_message_processor.h>
#include <nx/reflect/json.h>
#include <nx/utils/log/log.h>
#include <nx/vms/api/data/server_runtime_event_data.h>
#include <nx/vms/api/data/site_health_message.h>

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
    switch (eventData.eventType)
    {
        case ServerRuntimeEventType::undefined:
        {
            break;
        }
        case ServerRuntimeEventType::deviceAgentSettingsMaybeChanged:
        {
            DeviceAgentSettingsMaybeChangedData result;
            if (!NX_ASSERT(reflect::json::deserialize(nx::Buffer(eventData.eventData), &result)))
                return;

            emit deviceAgentSettingsMaybeChanged(
                result.deviceId,
                result.engineId);

            return;
        }
        case ServerRuntimeEventType::deviceFootageChanged:
        {
            DeviceFootageChangedData result;
            if (!NX_ASSERT(reflect::json::deserialize(nx::Buffer(eventData.eventData), &result)))
                return;

            emit deviceFootageChanged(result.deviceIds);

            return;
        }
        case ServerRuntimeEventType::analyticsStorageParametersChanged:
        {
            AnalyticsStorageParametersChangedData result;
            if (!NX_ASSERT(reflect::json::deserialize(nx::Buffer(eventData.eventData), &result)))
                return;

            emit analyticsStorageParametersChanged(result.serverId);

            return;
        }
        case ServerRuntimeEventType::deviceAdvancedSettingsManifestChanged:
        {
            DeviceAdvancedSettingsManifestChangedData result;
            if (!NX_ASSERT(reflect::json::deserialize(nx::Buffer(eventData.eventData), &result)))
                return;

            emit deviceAdvancedSettingsManifestChanged(result.deviceIds);

            return;
        }
        case ServerRuntimeEventType::serviceDisabled:
        {
            ServiceDisabledData result;
            if (!NX_ASSERT(reflect::json::deserialize(nx::Buffer(eventData.eventData), &result)))
                return;

            emit serviceDisabled(result.reason, result.deviceIds);

            return;
        }
        case ServerRuntimeEventType::siteHealthMessage:
        {
            nx::vms::api::SiteHealthMessage message;

            if (NX_ASSERT(nx::reflect::json::deserialize(
                nx::toBufferView(eventData.eventData), &message)))
            {
                emit(siteHealthMessage(message));
            }

            return;
        }
    }

    NX_WARNING(this, "Unexpected event type %1", eventData.eventType);
}

} // namespace nx::vms::client::core
