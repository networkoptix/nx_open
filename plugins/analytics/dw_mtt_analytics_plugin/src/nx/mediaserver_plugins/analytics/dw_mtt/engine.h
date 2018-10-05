#pragma once

#include <QtCore/QByteArray>

#include <plugins/plugin_tools.h>
#include <nx/sdk/analytics/engine.h>
#include <nx/sdk/analytics/device_agent.h>

#include "common.h"

namespace nx {
namespace mediaserver_plugins {
namespace analytics {
namespace dw_mtt {

/** Plugin for work with DWMTT-camera. */
class Engine: public nxpt::CommonRefCounter<nx::sdk::analytics::Engine>
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

    const EventType* eventTypeById(const QString& id) const noexcept;
 
    virtual void setDeclaredSettings(const nxpl::Setting* settings, int count) override;
    virtual void executeAction(
        nx::sdk::analytics::Action* action, nx::sdk::Error* outError) override;

private:
    QByteArray m_manifest;
    EngineManifest m_typedManifest;
};

} // namespace dw_mtt
} // namespace analytics
} // namespace mediaserver_plugins
} // namespace nx
