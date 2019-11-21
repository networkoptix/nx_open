#pragma once

#include <vector>
#include<string_view>

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/helpers/string_map.h>
#include <nx/sdk/analytics/i_device_agent.h>

#include <nx/network/http/http_client.h>

#include "engine.h"
#include "metadata_monitor.h"
#include "settings.h"

namespace nx::vms_server_plugins::analytics::hanwha {

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
    static void replanishErrorMap(nx::sdk::Ptr<nx::sdk::StringMap>& errorMap,
        AnalyticsParamSpan params, const char* reason);

    std::vector<std::string> ReadSettingsFromServer(
        const nx::sdk::IStringMap* settings,
        AnalyticsParamSpan analyticsParamSpan,
        int objectIndex = 0);

    std::string sendCommand(const std::string& query);
    std::string sendCommand2(const std::string& query);

    std::string WriteSettingsToCamera(
        const std::vector<std::string>& values,
        AnalyticsParamSpan analyticsParamSpan,
        const char* commandPreambule,
        int objectIndex = 0);

    std::string WriteSettingsToCamera(
        const char* query,
        int objectIndex = 0);

    template<class S>
    void retransmitSettings(
        nx::sdk::Ptr<nx::sdk::StringMap>& errorMap,
        const nx::sdk::IStringMap* settings,
        S& previousState,
        int objectIndex = 0)
    {
        const std::vector<std::string> values =
            ReadSettingsFromServer(settings, S::kParams, objectIndex);
        static constexpr const char* failedToReceive = "Failed to receive a value from server";

        if (values.empty())
        {
            replanishErrorMap(errorMap, S::kParams, failedToReceive);
            return;
        }

        S newState(values);
        if (!newState || (newState == previousState))
            return;

        std::string error;
        if constexpr (maybeEmpty<S>::value)
        {
            if (newState.empty())
                error = WriteSettingsToCamera(S::kAlternativeCommand, objectIndex);
            else
                error = WriteSettingsToCamera(values, S::kParams, S::kPreambule, objectIndex);
        }
        else
        {
            error = WriteSettingsToCamera(values, S::kParams, S::kPreambule, objectIndex);
        }
        if (!error.empty())
            replanishErrorMap(errorMap, S::kParams, error.c_str());
        else
            previousState = newState;

    }

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

    Settings m_settings;
    FrameSize m_frameSize = { 3840, 2160 };
    nx::network::http::HttpClient m_settingsHttpClient;
};

} // namespace nx::vms_server_plugins::analytics::hanwha
