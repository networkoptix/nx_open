#pragma once

#include <QtCore/QByteArray>

#include <nx/sdk/helpers/ref_countable.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/analytics/i_engine.h>
#include <nx/sdk/analytics/i_device_agent.h>

#include "common.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace vca {

/**
 * Plugin for working with VCA camera. Deals with three events: motion-detected, vca-event and
 * face-detected.
 */
class Engine: public nx::sdk::RefCountable<nx::sdk::analytics::IEngine>
{
public:
    Engine(nx::sdk::analytics::Plugin* plugin);

    virtual void setEngineInfo(const nx::sdk::analytics::IEngineInfo* engineInfo) override;

    const EventType* eventTypeById(const QString& id) const noexcept;

    virtual void setHandler(nx::sdk::analytics::IEngine::IHandler* handler) override;

    virtual bool isCompatible(const nx::sdk::IDeviceInfo* deviceInfo) const override;

protected:
    virtual void doSetSettings(
        nx::sdk::Result<const nx::sdk::IStringMap*>* outResult,
        const nx::sdk::IStringMap* settings) override;
    virtual void getPluginSideSettings(
        nx::sdk::Result<const nx::sdk::ISettingsResponse*>* outResult) const override;
    virtual void getManifest(nx::sdk::Result<const nx::sdk::IString*>* outResult) const override;
    virtual void doObtainDeviceAgent(
        nx::sdk::Result<nx::sdk::analytics::IDeviceAgent*>* outResult,
        const nx::sdk::IDeviceInfo* deviceInfo) override;
    virtual void doExecuteAction(nx::sdk::Result<void>* outResult,
        nx::sdk::analytics::IAction* action) override;

private:
    nx::sdk::analytics::Plugin* const m_plugin;

    QByteArray m_manifest;
    EngineManifest m_typedManifest;
};

} // namespace vca
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
