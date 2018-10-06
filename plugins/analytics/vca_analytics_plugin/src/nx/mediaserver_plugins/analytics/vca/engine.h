#pragma once

#include <QtCore/QByteArray>

#include <plugins/plugin_tools.h>

#include <nx/sdk/analytics/common_plugin.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace vca {

/**
 * Plugin for working with VCA camera. Deals with three events: motion-detected, vca-event and
 * face-detected.
 */
class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::Engine>
{
public:
    Engine(nx::sdk::analytics::CommonPlugin* plugin);

    virtual nx::sdk::analytics::CommonPlugin* plugin() const override { return m_plugin; }

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;

    virtual nx::sdk::analytics::DeviceAgent* obtainDeviceAgent(
        const nx::sdk::DeviceInfo* deviceInfo,
        nx::sdk::Error* outError) override;

    virtual const char* manifest(
        nx::sdk::Error* error) const override;

    const EventType* eventTypeById(const QString& id) const noexcept;

    virtual void executeAction(
        nx::sdk::analytics::Action* action, nx::sdk::Error* outError) override;

private:
    nx::sdk::analytics::CommonPlugin* const m_plugin;

    QByteArray m_manifest;
    EngineManifest m_typedManifest;
};

} // namespace vca
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
