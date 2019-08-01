#pragma once

#include <QtCore/QByteArray>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>
#include <nx/sdk/analytics/helpers/result_aliases.h>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace dw_mtt {

/** Plugin for work with DWMTT-camera. */
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

    const EventType* eventTypeById(const QString& id) const noexcept;

    virtual nx::sdk::Result<void> executeAction(nx::sdk::analytics::IAction* action) override;

    virtual void setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

private:
    nx::sdk::analytics::Plugin* const m_plugin;

    QByteArray m_manifest;
    EngineManifest m_typedManifest;
};

} // namespace dw_mtt
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
