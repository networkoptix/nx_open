#pragma once

#include <stdint.h>
#include <vector>

#include <QtCore/QObject>
#include <QtCore/QUrl>
#include <QtCore/QString>
#include <QtNetwork/QAuthenticator>

#include <nx/utils/thread/mutex.h>
#include <nx/utils/url.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/i_plugin_diagnostic_event.h>

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

    const EngineManifest& events() const noexcept
    {
        return m_parsedManifest;
    }

    /** @return nullptr if not found. */
    const EventType* eventTypeById(const QString& id) const noexcept;

    void pushPluginDiagnosticEvent(
        nx::sdk::IPluginDiagnosticEvent::Level level,
        std::string caption,
        std::string description);
        
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
        
private:
    nx::sdk::Result<void> startFetchingMetadata();
    void stopFetchingMetadata();

private:
    Engine* const m_engine;
    EngineManifest m_parsedManifest;
    QByteArray m_jsonManifest;
    QUrl m_url;
    std::uint16_t m_localMetadataPort = 0;
    nx::utils::Url m_externalMetadataUrl;
    QAuthenticator m_auth;
    Monitor* m_monitor = nullptr;

    nx::sdk::Ptr<nx::sdk::analytics::IDeviceAgent::IHandler> m_handler;
    nx::sdk::Ptr<nx::sdk::analytics::IMetadataTypes> m_neededMetadataTypes;
};

} // nx::vms_server_plugins::analytics::axis
