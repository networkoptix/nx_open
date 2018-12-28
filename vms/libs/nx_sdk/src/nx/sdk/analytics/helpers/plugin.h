#pragma once

#include <string>
#include <functional>

#include <plugins/plugin_tools.h>
#include <nx/sdk/helpers/log_utils.h>
#include <nx/sdk/error.h>

#include <nx/sdk/analytics/i_plugin.h>

#include "engine.h"

namespace nx {
namespace sdk {
namespace analytics {

/**
 * Base class for a typical implementation of an Analytics Plugin. Hides many technical details of
 * the Analytics Plugin SDK, but may limit plugin capabilities - use only when suitable.
 */
class Plugin: public nxpt::CommonRefCounter<IPlugin>
{
public:
    using CreateEngine = std::function<IEngine*(IPlugin* plugin)>;

    /**
     * @param libName Name of the plugin library. It's needed for the logging.
     * @param pluginManifest Plugin manifest to be returned from the manifest method.
     * @param createEngine Functor for engine creation.
     */
    Plugin(
        std::string libName,
        std::string pluginManifest,
        CreateEngine createEngine);

    virtual ~Plugin() override;

    nxpl::PluginInterface* pluginContainer() const { return m_pluginContainer; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* name() const override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;
    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    virtual const IString* manifest(Error* outError) const override;

    virtual IEngine* createEngine(Error* outError) override;

private:
    const std::string m_name;
    const std::string m_manifest;

    CreateEngine m_createEngine;
    nxpl::PluginInterface* m_pluginContainer;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
