#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_device_agent.h>

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

    virtual void setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes,
        nx::sdk::IError* outError) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::IError* outError) const override;

    void setDeviceInfo(const nx::sdk::IDeviceInfo* deviceInfo);
    void setDeviceAgentManifest(const QByteArray& manifest);
    void setEngineManifest(const Hanwha::EngineManifest& manifest);

    void setMonitor(MetadataMonitor* monitor);

    virtual void setSettings(
        const nx::sdk::IStringMap* settings,
        nx::sdk::IError* outError) override;

    virtual nx::sdk::IStringMap* pluginSideSettings(nx::sdk::IError* outError) const override;

private:
    void startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes,
        nx::sdk::IError* outError);

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
