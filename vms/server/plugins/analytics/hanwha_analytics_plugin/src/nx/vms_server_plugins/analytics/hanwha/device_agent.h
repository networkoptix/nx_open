#pragma once

#include <vector>
#include <mutex>
#include <string_view>

#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/i_string_map.h>
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
#include "settings_capabilities.h"
#include "settings_processor.h"
#include "object_metadata_xml_parser.h"
#include "value_transformer.h"

namespace nx::vms_server_plugins::analytics::hanwha {

class DeviceAgent:
    public nx::sdk::RefCountable<nx::sdk::analytics::IConsumingDeviceAgent>
{
public:
    DeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo,
        Engine* engine,
        const SettingsCapabilities& settingsCapabilities,
        RoiResolution roiResolution,
        bool isNvr,
        const Hanwha::DeviceAgentManifest& deviceAgentManifest);

    virtual ~DeviceAgent();

    virtual void setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* IHandler) override;

    void setDeviceInfo(const nx::sdk::IDeviceInfo* deviceInfo);

    void setMonitor(MetadataMonitor* monitor);

    void loadAndHoldDeviceSettings();

protected:
    void setSupportedEventCategoties();

    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult,
        const nx::sdk::IStringMap* settings) override;
    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual void doSetNeededMetadataTypes(
        nx::sdk::Result<void>* outValue,
        const nx::sdk::analytics::IMetadataTypes* neededMetadataTypes) override;

    virtual void doPushDataPacket(
        nx::sdk::Result<void>* outResult, nx::sdk::analytics::IDataPacket* dataPacket) override;

    virtual void finalize() override {}

private:
    nx::sdk::Result<void> startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

    void applyWearingMaskBoundingBoxColorSettings(const nx::sdk::IStringMap* settings);

public:
    std::string loadFirmwareVersion() const;

    const SettingsCapabilities& settingsCapabilities() { return m_settingsCapabilities; }
private:
    Engine* const m_engine;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channelNumber = 0;

    MetadataMonitor* m_monitor = nullptr;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;

    const SettingsCapabilities m_settingsCapabilities;
    RoiResolution m_roiResolution;
    Settings m_settings;
    mutable SettingsProcessor m_settingsProcessor;
    Hanwha::DeviceAgentManifest m_manifest;

    mutable std::mutex m_settingsMutex;
    bool m_serverHasSentInitialSettings = false;

    ObjectMetadataXmlParser m_objectMetadataXmlParser;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
