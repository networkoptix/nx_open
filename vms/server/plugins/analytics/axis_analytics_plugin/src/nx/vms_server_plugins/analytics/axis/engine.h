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
#include <nx/network/socket_global.h>

#include "common.h"

namespace nx::vms_server_plugins::analytics::axis {

class Engine: public nx::sdk::RefCountable<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);

    virtual nx::sdk::analytics::Plugin* plugin() const override { return m_plugin; }

    virtual void setEngineInfo(const nx::sdk::analytics::IEngineInfo* engineInfo) override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::IDeviceInfo* deviceInfo,
        nx::sdk::Error* outError) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual void executeAction(
        nx::sdk::analytics::IAction* action, nx::sdk::Error* outError) override;

    virtual nx::sdk::Error setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

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
