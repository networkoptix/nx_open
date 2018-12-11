#pragma once

#include <QtCore/QByteArray>

#include <plugins/plugin_tools.h>

#include <nx/sdk/analytics/common/plugin.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace vca {

/**
 * Plugin for working with VCA camera. Deals with three events: motion-detected, vca-event and
 * face-detected.
 */
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

    const EventType* eventTypeById(const QString& id) const noexcept;

    virtual void executeAction(
        nx::sdk::analytics::Action* action, nx::sdk::Error* outError) override;

    virtual nx::sdk::Error setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

private:
    nx::sdk::analytics::common::Plugin* const m_plugin;

    QByteArray m_manifest;
    EngineManifest m_typedManifest;
};

} // namespace vca
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
