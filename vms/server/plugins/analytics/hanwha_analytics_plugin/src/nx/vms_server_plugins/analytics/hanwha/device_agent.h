#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/aliases.h>

#include "engine.h"
#include "metadata_monitor.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

class DeviceAgent:
    public QObject,
    public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
    Q_OBJECT

public:
    DeviceAgent(Engine* engine);
    virtual ~DeviceAgent();

    virtual Engine* engine() const override { return m_engine; }

    virtual void setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* IHandler) override;

    virtual nx::sdk::VoidResult setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual nx::sdk::StringResult manifest() const override;

    void setDeviceInfo(const nx::sdk::IDeviceInfo* deviceInfo);
    void setDeviceAgentManifest(const QByteArray& manifest);
    void setEngineManifest(const Hanwha::EngineManifest& manifest);

    void setMonitor(MetadataMonitor* monitor);

    virtual nx::sdk::StringMapResult setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::SettingsResponseResult pluginSideSettings() const override;

private:
    nx::sdk::VoidResult startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

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
    int m_channelNumber = 0;

    MetadataMonitor* m_monitor = nullptr;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;
};

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
