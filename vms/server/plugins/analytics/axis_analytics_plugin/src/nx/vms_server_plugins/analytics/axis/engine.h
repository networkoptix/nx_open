#pragma once

#include <vector>

#include <boost/optional/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/helpers/result_aliases.h>
#include <nx/network/socket_global.h>

#include "common.h"

namespace nx::vms_server_plugins::analytics::axis {

class Engine: public nx::sdk::RefCountable<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);

    virtual void setEngineInfo(const nx::sdk::analytics::IEngineInfo* engineInfo) override;

    virtual nx::sdk::StringMapResult setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::SettingsResponseResult pluginSideSettings() const override;

    virtual nx::sdk::analytics::MutableDeviceAgentResult obtainDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo) override;

    virtual nx::sdk::StringResult manifest() const override;

    virtual nx::sdk::Result<void> executeAction(nx::sdk::analytics::IAction* action) override;

    virtual void setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

private:
    EngineManifest fetchSupportedEvents(const nx::sdk::IDeviceInfo* deviceInfo);

private:
    nx::sdk::analytics::Plugin* const m_plugin;

    // Engine manifest is stored in serialized and deserialized states, since both of them needed.
    QByteArray m_jsonManifest;
    EngineManifest m_parsedManifest;
};

} // nx::vms_server_plugins::analytics::axis
