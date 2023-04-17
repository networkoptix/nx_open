// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <nx/sdk/analytics/helpers/consuming_device_agent.h>

#include "engine.h"
#include "stub_analytics_plugin_events_ini.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace events {

const std::string kDeclareAdditionalEventTypesSetting = "declareAdditionalEventTypesSetting";
const std::string kGenerateEventsSetting = "generateEvents";

class DeviceAgent: public nx::sdk::analytics::ConsumingDeviceAgent
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent() override;

protected:
    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual std::string manifestString() const override;

    virtual nx::sdk::Result<const nx::sdk::ISettingsResponse*> settingsReceived() override;

private:
    nx::sdk::analytics::IMetadataPacket* cookSomeEvents();
    int64_t usSinceEpoch() const;
    void startFetchingMetadata(const nx::sdk::analytics::IMetadataTypes* metadataTypes);
    void stopFetchingMetadata();
    void parseSettings();
    void eventThreadLoop();
    void startEventThread();
    void stopEventThread();

private:
    Engine* const m_engine;

    std::unique_ptr<std::thread> m_eventThread;
    std::mutex m_eventThreadMutex;
    std::condition_variable m_eventThreadCondition;
    std::atomic<bool> m_terminated{false};
    std::atomic<bool> m_needToGenerateEvents{false};
    std::string m_eventTypeId;

    struct DeviceAgentSettings
    {
        std::atomic<bool> declareAdditionalEventTypes{false};
        std::atomic<bool> generateEvents{true};
    };

    DeviceAgentSettings m_deviceAgentSettings;

    struct EventContext
    {
        int currentEventTypeIndex = 0;
    };

    EventContext m_eventContext;
};

const std::string kLineCrossingEventType = "nx.stub.lineCrossing";
const std::string kObjectInTheAreaEventType = "nx.stub.objectInTheArea";
const std::string kLoiteringEventType = "nx.stub.loitering";
const std::string kIntrusionEventType = "nx.stub.intrusion";
const std::string kGunshotEventType = "nx.stub.gunshot";
const std::string kSuspiciousNoiseEventType = "nx.stub.suspiciousNoise";
const std::string kSoundRelatedEventGroup = "nx.stub.soundRelated";
const std::string kAdditionalEventType = "nx.stub.additionalEvent1";
const std::string kAdditionalEventType2 = "nx.stub.additionalEvent2";

} // namespace events
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
