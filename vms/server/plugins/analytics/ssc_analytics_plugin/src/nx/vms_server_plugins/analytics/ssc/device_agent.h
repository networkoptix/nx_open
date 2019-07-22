#pragma once

#include <memory>

#include <QtCore/QUrl>
#include <QtCore/QByteArray>

#include <nx/sdk/helpers/ref_countable.h>

#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/result_aliases.h>

#include "common.h"
#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace ssc {

class DeviceAgent: public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
public:
    DeviceAgent(
        Engine* engine,
        const nx::sdk::IDeviceInfo* deviceInfo,
        const EngineManifest& typedManifest);

    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    void sendEventPacket(const EventType& event) const;

    virtual nx::sdk::StringResult manifest() const override;

    virtual void setHandler(nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    virtual nx::sdk::Result<void> setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual nx::sdk::StringMapResult setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::SettingsResponseResult pluginSideSettings() const override;

private:
    void startFetchingMetadata(const nx::sdk::analytics::IMetadataTypes* metadataTypes);
    void stopFetchingMetadata();

private:
    Engine* const m_engine;
    const QUrl m_url;
    const int m_cameraLogicalId = 0;
    QByteArray m_deviceAgentManifest;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;
    mutable uint64_t m_packetId = 0; //< autoincrement packet number for log and debug
};

} // namespace ssc
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
