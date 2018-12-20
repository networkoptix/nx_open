#pragma once

#include <vector>

#include <boost/optional/optional.hpp>

#include <QtCore/QByteArray>
#include <QtCore/QUrl>
#include <QtNetwork/QAuthenticator>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/common/plugin.h>
#include <nx/network/socket_global.h>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace axis {

class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::common::Plugin* plugin);

    virtual nx::sdk::analytics::common::Plugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nx::sdk::IStringMap* settings) override;

    virtual nx::sdk::IStringMap* pluginSideSettings() const override;

    virtual nx::sdk::analytics::IDeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo,
        nx::sdk::Error* outError) override;

    virtual const nx::sdk::IString* manifest(nx::sdk::Error* error) const override;

    virtual void executeAction(
        nx::sdk::analytics::Action* action, nx::sdk::Error* outError) override;

    virtual nx::sdk::Error setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::DeviceInfo* deviceInfo) const override;

private:
    EngineManifest fetchSupportedEvents(
        const nx::sdk::DeviceInfo& deviceInfo);

private:
    nx::sdk::analytics::common::Plugin* const m_plugin;
    EngineManifest m_typedManifest;
    QByteArray m_manifest;
};

} // namespace axis
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
