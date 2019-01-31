#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>

#include "metadata_monitor.h"
#include "engine.h"

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/utils/url.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

class DeviceAgent:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::analytics::IDeviceAgent>
{
    Q_OBJECT

public:
    DeviceAgent(Engine* engine);

    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    void setDeviceInfo(const nx::sdk::IDeviceInfo* deviceInfo);
    void setDeviceAgentManifest(const QByteArray& manifest);
    void setEngineManifest(const Hikvision::EngineManifest& manifest);
    virtual void setSettings(const nx::sdk::IStringMap* settings) override;
    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

private:
    nx::sdk::Error startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

private:
    Engine* const m_engine;

    Hikvision::EngineManifest m_engineManifest;
    QByteArray m_deviceAgentManifest;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channelNumber = 0;

    std::unique_ptr<HikvisionMetadataMonitor> m_monitor;
    nx::sdk::analytics::IDeviceAgent::IHandler* m_handler = nullptr;
};

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
