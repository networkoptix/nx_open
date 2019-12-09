#pragma once

#include <vector>
#include<string_view>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/consuming_device_agent.h>

#include <nx/network/http/http_client.h>

#include "engine.h"
#include "metadata_monitor.h"
#include "settings.h"

namespace nx::vms_server_plugins::analytics::hanwha {

class DeviceAgent:
    public QObject,
    public nx::sdk::RefCountable<nx::sdk::analytics::IConsumingDeviceAgent>
{
    Q_OBJECT

public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo);
    virtual ~DeviceAgent();

    virtual void setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* IHandler) override;

    void setDeviceInfo(const nx::sdk::IDeviceInfo* deviceInfo);
    void setDeviceAgentManifest(const QByteArray& manifest);
    void setMonitor(MetadataMonitor* monitor);

    void readCameraSettings();

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::IStringMap*>* outResult,
        const nx::sdk::IStringMap* settings) override;
    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual void doPushDataPacket(
        nx::sdk::Result<void>* outResult, nx::sdk::analytics::IDataPacket* dataPacket) override;

private:
    nx::sdk::Result<void> startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

    std::string sendWritingRequestToDeviceSync(const std::string& query);

    std::string sendReadingRequestToDeviceSync(
        const char* domain, const char* submenu, const char* action);

    std::string loadEventSettings(const char* eventName);

    void loadFrameSize();

    void loadSupportedEventTypes(); // page 9 Application Programmers Guide

private:

    Engine* const m_engine;

    // Json string representation of nx::vms::api::analytics::DeviceAgentManifest.
    QByteArray m_manifest;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channelNumber = 0;

    MetadataMonitor* m_monitor = nullptr;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;

    Settings m_settings;
    FrameSize m_frameSize;
    nx::network::http::HttpClient m_settingsHttpClient;
    bool m_serverHasSentInitialSettings = false;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
