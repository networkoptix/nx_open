#pragma once

#include <memory>

#include <QtCore/QUrl>
#include <QtCore/QByteArray>

#include <plugins/plugin_tools.h>

#include <nx/sdk/analytics/device_agent.h>

#include "common.h"
#include "engine.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace ssc {

class DeviceAgent: public nxpt::CommonRefCounter<nx::sdk::analytics::DeviceAgent>
{
public:
    DeviceAgent(
        Engine* engine,
        const nx::sdk::DeviceInfo& deviceInfo,
        const EngineManifest& typedManifest);

    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    void sendEventPacket(const EventType& event) const;

    virtual nx::sdk::Error startFetchingMetadata(
        const char* const* tpeList, int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* manifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    virtual sdk::Error setMetadataHandler(nx::sdk::analytics::MetadataHandler* metadataHandler) override;

    virtual void setSettings(const nxpl::Setting* /*settings*/, int /*count*/) override {}

private:
    Engine* const m_engine;
    const QUrl m_url;
    const int m_cameraLogicalId;
    QByteArray m_deviceAgentManifest;
    nx::sdk::analytics::MetadataHandler* m_metadataHandler = nullptr;
    mutable uint64_t m_packetId = 0; //< autoincrement packet number for log and debug
};

} // namespace ssc
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
