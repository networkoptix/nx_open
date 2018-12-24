#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/utils/url.h>

#include "engine.h"
#include "metadata_monitor.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dahua {

class DeviceAgent:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::analytics::IDeviceAgent>
{
public:
    DeviceAgent(Engine* engine,
        const nx::sdk::DeviceInfo& deviceInfo,
        const nx::vms::api::analytics::DeviceAgentManifest& deviceAgentParsedManifest);

    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

private:
    void setDeviceInfo(const nx::sdk::DeviceInfo& deviceInfo);

    nx::sdk::Error startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

private:
    Engine* const m_engine;

    // Device Agent manifest is stored in serialized and deserialized states, since both of them
    // needed.
    const QByteArray m_jsonManifest;
    const nx::vms::api::analytics::DeviceAgentManifest m_parsedManifest;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channel = 0;

    std::unique_ptr<MetadataMonitor> m_monitor;
    nx::sdk::analytics::IDeviceAgent::IHandler* m_handler = nullptr;
};

} // namespace dahua
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
