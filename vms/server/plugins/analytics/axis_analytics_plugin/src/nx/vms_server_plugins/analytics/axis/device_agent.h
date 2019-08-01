#pragma once

#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>
#include <nx/sdk/analytics/helpers/result_aliases.h>

#include "common.h"
#include "monitor.h"
#include "engine.h"

namespace nx::vms_server_plugins::analytics::axis {

class DeviceAgent: public nx::sdk::RefCountable<nx::sdk::analytics::IDeviceAgent>
{
public:
    DeviceAgent(
        Engine* engine,
        const nx::sdk::IDeviceInfo* deviceInfo,
        const EngineManifest& typedManifest);

    virtual ~DeviceAgent();

    virtual void setHandler(nx::sdk::analytics::IDeviceAgent::IHandler* handler) override;

    virtual nx::sdk::Result<void> setNeededMetadataTypes(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes) override;

    virtual nx::sdk::StringResult manifest() const override;

    const EngineManifest& events() const noexcept
    {
        return m_parsedManifest;
    }

    virtual nx::sdk::StringMapResult setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::SettingsResponseResult pluginSideSettings() const override;

    /** @return nullptr if not found. */
    const EventType* eventTypeById(const QString& id) const noexcept;

    void pushPluginDiagnosticEvent(
        nx::sdk::IPluginDiagnosticEvent::Level level,
        std::string caption,
        std::string description);

private:
    nx::sdk::Result<void> startFetchingMetadata(
        const nx::sdk::analytics::IMetadataTypes* metadataTypes);
    void stopFetchingMetadata();

private:
    Engine* const m_engine;
    EngineManifest m_parsedManifest;
    QByteArray m_jsonManifest;
    QUrl m_url;
    QAuthenticator m_auth;
    Monitor* m_monitor = nullptr;

    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;
};

} // nx::vms_server_plugins::analytics::axis
