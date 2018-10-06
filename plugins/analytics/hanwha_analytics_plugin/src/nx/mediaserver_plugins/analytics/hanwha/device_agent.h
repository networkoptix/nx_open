#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/device_agent.h>

#include "engine.h"
#include "metadata_monitor.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace hanwha {

class DeviceAgent:
    public QObject,
    public nxpt::CommonRefCounter<nx::sdk::analytics::DeviceAgent>
{
    Q_OBJECT

public:
    DeviceAgent(Engine* engine);
    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual nx::sdk::Error setMetadataHandler(
        nx::sdk::analytics::MetadataHandler* metadataHandler) override;

    virtual nx::sdk::Error startFetchingMetadata(
        const char* const* typeList, int typeListSize) override;

    virtual nx::sdk::Error stopFetchingMetadata() override;

    virtual const char* manifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    void setDeviceInfo(const nx::sdk::DeviceInfo& deviceInfo);
    void setDeviceAgentManifest(const QByteArray& manifest);
    void setEngineManifest(const Hanwha::EngineManifest& manifest);

    void setMonitor(MetadataMonitor* monitor);

    virtual void setSettings(const nxpl::Setting* settings, int count) override;

private:
    Engine* const m_engine;

    Hanwha::EngineManifest m_engineManifest;
    QByteArray m_deviceAgentManifest;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channel = 0;

    MetadataMonitor* m_monitor = nullptr;
    nx::sdk::analytics::MetadataHandler* m_metadataHandler = nullptr;
};

} // namespace hanwha
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
