#pragma once

#include <memory>

#include <QtCore/QUrl>
#include <QtCore/QByteArray>

#include <plugins/plugin_tools.h>

#include <nx/sdk/analytics/i_device_agent.h>

#include "common.h"
#include "engine.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace ssc {

class DeviceAgent: public nxpt::CommonRefCounter<nx::sdk::analytics::IDeviceAgent>
{
public:
    DeviceAgent(
        Engine* engine,
        const nx::sdk::IDeviceInfo* deviceInfo,
        const EngineManifest& typedManifest);

    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    void sendEventPacket(const EventType& event) const;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual sdk::Error setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    virtual sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

private:
    nx::sdk::Error startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    nx::sdk::Error stopFetchingMetadata();

private:
    Engine* const m_engine;
    const QUrl m_url;
    const int m_cameraLogicalId = 0;
    QByteArray m_deviceAgentManifest;
    nx::sdk::analytics::IDeviceAgent::IHandler* m_handler = nullptr;
    mutable uint64_t m_packetId = 0; //< autoincrement packet number for log and debug
};

} // namespace ssc
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
