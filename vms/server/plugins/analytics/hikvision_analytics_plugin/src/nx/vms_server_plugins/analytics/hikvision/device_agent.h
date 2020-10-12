#pragma once

#include <mutex>
#include <optional>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/i_consuming_device_agent.h>
#include <nx/utils/url.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/time_helper.h>

#include "metadata_monitor.h"
#include "engine.h"
#include "metadata_parser.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hikvision {

class DeviceAgent:
    public QObject,
    public nx::sdk::RefCountable<nx::sdk::analytics::IConsumingDeviceAgent>
{
    Q_OBJECT

public:
    DeviceAgent(Engine* engine);

    virtual ~DeviceAgent();

    virtual void setHandler(nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    void setDeviceInfo(const nx::sdk::IDeviceInfo* deviceInfo);
    void setDeviceAgentManifest(const QByteArray& manifest);
    void setEngineManifest(const Hikvision::EngineManifest& manifest);

protected:
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
        nx::sdk::Result<void>* outResult,
        nx::sdk::analytics::IDataPacket* dataPacket) override;

    virtual void finalize() override {}

private:
    nx::sdk::Result<void> startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

private:
    Engine* const m_engine;

    Hikvision::EngineManifest m_engineManifest;
    QByteArray m_deviceAgentManifest;

    nx::utils::Url m_url;
    QString m_model;
    QString m_firmware;
    QString m_name;
    QAuthenticator m_auth;
    QString m_uniqueId;
    QString m_sharedId;
    int m_channelNumber = 0;

    std::unique_ptr<HikvisionMetadataMonitor> m_monitor;
    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;

    std::mutex m_metadataParserMutex;
    MetadataParser m_metadataParser;
    std::optional<nx::utils::TimeHelper> m_eventTimestampAdjuster;
};

} // namespace hikvision
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
