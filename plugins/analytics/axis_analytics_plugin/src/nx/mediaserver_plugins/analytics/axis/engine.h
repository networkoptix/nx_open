#pragma once

#include <vector>

#include <boost/optional/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>
#include <nx/network/socket_global.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace axis {

class Engine:
    public nxpt::CommonRefCounter<nx::sdk::analytics::Engine>
{
public:
    Engine();

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* name() const override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    virtual void setLocale(const char* locale) override;

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo,
        nx::sdk::Error* outError) override;

    virtual const char* manifest(
        nx::sdk::Error* error) const override;

    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;

    virtual void executeAction(
        nx::sdk::analytics::Action* action, nx::sdk::Error* outError) override;

private:
    EngineManifest fetchSupportedEvents(
        const nx::sdk::DeviceInfo& deviceInfo);

private:
    EngineManifest m_typedManifest;
    QByteArray m_manifest;
};

} // namespace axis
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
