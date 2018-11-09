#pragma once

#include <vector>

#include <boost/optional/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/common_plugin.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/network/socket_global.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace axis {

class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::Engine>
{
public:
    Engine(nx::sdk::analytics::CommonPlugin* plugin);

    virtual nx::sdk::analytics::CommonPlugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nx::sdk::Settings* settings) override;

    virtual nx::sdk::Settings* settings() const override;

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo,
        nx::sdk::Error* outError) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual void executeAction(
        nx::sdk::analytics::Action* action, nx::sdk::Error* outError) override;

private:
    EngineManifest fetchSupportedEvents(
        const nx::sdk::DeviceInfo& deviceInfo);

private:
    nx::sdk::analytics::CommonPlugin* const m_plugin;
    EngineManifest m_typedManifest;
    QByteArray m_manifest;
};

} // namespace axis
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
