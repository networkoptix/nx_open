#pragma once

#include <string>
#include <map>
#include <functional>

#include <plugins/plugin_tools.h>
#include <nx/sdk/utils.h>
#include <nx/sdk/common.h>

#include "plugin.h"
#include "engine.h"
#include "objects_metadata_packet.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Base class for a typical implementation of an Analytics Plugin. Hides many technical details of
 * the Analytics Plugin SDK, but may limit plugin capabilities - use only when suitable.
 */
class CommonPlugin: public nxpt::CommonRefCounter<Plugin>
{
public:
    using CreateEngine = std::function<Engine*(Plugin* plugin)>;

    /**
     * @param pluginManifest Plugin manifest to be returned from the manifest method.
     * @param createEngine Functor for engine creation.
     */
    CommonPlugin(
        // TODO: Actually we can figure out the plugin name from the manifest,
        // But since there is no JSON support in our helpers let's pass it as an additional
        // parameter. Probably we need to add some JSON helpers to the SDK.
        std::string pluginName,
        std::string pluginManifest,
        CreateEngine createEngine);

    virtual ~CommonPlugin() override;

    nxpl::PluginInterface* pluginContainer() const { return m_pluginContainer; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* name() const override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;
    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    virtual const char* manifest(nx::sdk::Error* outError) const override;

    virtual void freeManifest(const char* data) override;

    virtual Engine* createEngine(Error* outError) override;

private:
    const std::string m_name;
    const std::string m_manifest;

    CreateEngine m_createEngine;
    nxpl::PluginInterface* m_pluginContainer;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
