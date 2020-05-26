#pragma once

#include <vector>
#include <mutex>
#include <string_view>

#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>
#include <nx/sdk/analytics/helpers/consuming_device_agent.h>
#include <nx/vms/api/analytics/device_agent_manifest.h>

#include <nx/network/http/http_client.h>

#include "engine.h"
#include "metadata_monitor.h"
#include "settings.h"
#include "settings_processor.h"
#include "object_metadata_xml_parser.h"
#include "value_transformer.h"

namespace nx::vms_server_plugins::analytics::hanwha {

class DeviceAgent:
    public nx::sdk::RefCountable<nx::sdk::analytics::IConsumingDeviceAgent>
{
public:
    DeviceAgent(Engine* engine, const nx::sdk::IDeviceInfo* deviceInfo,
        bool isNvr, QSize maxResolution);
    virtual ~DeviceAgent();

    virtual void setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* IHandler) override;

    void setDeviceInfo(const nx::sdk::IDeviceInfo* deviceInfo);
    void setManifest(Hanwha::DeviceAgentManifest manifest);

    void setMonitor(MetadataMonitor* monitor);

    void loadAndHoldDeviceSettings();

protected:
    void setSupportedEventCategoties();

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

public:
    std::optional<QSet<QString>> loadRealSupportedEventTypes() const;

    std::string loadFirmwareVersion() const;

private:
    Engine* const m_engine;

    Hanwha::DeviceAgentManifest m_manifest;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channelNumber = 0;

    MetadataMonitor* m_monitor = nullptr;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;

    FrameSize m_frameSize;
    Settings m_settings;
    SettingsProcessor m_settingsProcessor;
    mutable std::mutex m_settingsMutex;
    bool m_serverHasSentInitialSettings = false;

    ObjectMetadataXmlParser m_objectMetadataXmlParser;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
