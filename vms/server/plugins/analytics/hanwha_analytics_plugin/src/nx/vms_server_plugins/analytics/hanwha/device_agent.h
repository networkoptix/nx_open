#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/analytics/i_device_agent.h>

#include <nx/network/http/http_client.h>

#include "engine.h"
#include "metadata_monitor.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace hanwha {

struct AnalyticsParam
{
    const char* plugin;
    const char* sunapi;
};

/** temporary crutch till C++20 std::span */
class AnalyticsParamSpan
{
public:
    template<int N>
    AnalyticsParamSpan(const AnalyticsParam(&params)[N]) noexcept:
        m_begin(params),
        m_end(params + std::size(params))
    {
    }
    const AnalyticsParam* begin() { return m_begin; }
    const AnalyticsParam* end() { return m_end; }

private:
    const AnalyticsParam* m_begin;
    const AnalyticsParam* m_end;
};

class DeviceAgent:
    public QObject,
    public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
    Q_OBJECT

public:
    DeviceAgent(Engine* engine);
    virtual ~DeviceAgent();

    virtual void setHandler(
        nx::sdk::analytics::IDeviceAgent::IHandler* IHandler) override;

    void setDeviceInfo(const nx::sdk::IDeviceInfo* deviceInfo);
    void setDeviceAgentManifest(const QByteArray& manifest);
    void setEngineManifest(const Hanwha::EngineManifest& manifest);

    void setMonitor(MetadataMonitor* monitor);

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

private:
    nx::sdk::Result<void> startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);

    void stopFetchingMetadata();

private:

    void updateCameraEventSetting(
        const nx::sdk::IStringMap* settings,
        const char* kCommandPreambule,
        AnalyticsParamSpan analyticsParamSpan, //< TODO: update in C++20
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap);

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

    nx::network::http::HttpClient m_settingsHttpClient;
};

} // namespace hanwha
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
