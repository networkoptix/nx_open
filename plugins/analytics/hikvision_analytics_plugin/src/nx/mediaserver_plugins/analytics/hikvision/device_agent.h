#pragma once

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>

#include "metadata_monitor.h"
#include "engine.h"

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/utils/url.h>

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace hikvision {

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

    virtual nx::sdk::Error setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual const char* manifest(nx::sdk::Error* error) override;

    virtual void freeManifest(const char* data) override;

    void setDeviceInfo(const nx::sdk::DeviceInfo& deviceInfo);
    void setDeviceAgentManifest(const QByteArray& manifest);
    void setEngineManifest(const Hikvision::EngineManifest& manifest);
    virtual void setSettings(const nx::sdk::Settings* settings) override;
    virtual nx::sdk::Settings* settings() const override;

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
    int m_channel = 0;

    std::unique_ptr<HikvisionMetadataMonitor> m_monitor;
    nx::sdk::analytics::MetadataHandler* m_metadataHandler = nullptr;
};

} // namespace hikvision
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
