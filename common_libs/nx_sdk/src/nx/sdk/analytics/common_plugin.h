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
     * @param pluginName Full plugin name in English, capitalized as a header.
     * @param libName Short plugin name: small_letters_and_underscores, as in the library filename.
     */
    CommonPlugin(std::string libName, CreateEngine createEngine);

    virtual ~CommonPlugin() override;

    nxpl::PluginInterface* pluginContainer() const { return m_pluginContainer; }

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    /** @return libName. */
    virtual const char* name() const override;

    virtual void setSettings(const nxpl::Setting* settings, int count) override;
    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;
    virtual void setLocale(const char* locale) override;

    virtual Engine* createEngine(Error* outError);

private:
    const std::string m_libName;
    CreateEngine m_createEngine;
    nxpl::PluginInterface* m_pluginContainer;
};

} // namespace analytics
} // namespace sdk
} // namespace nx
