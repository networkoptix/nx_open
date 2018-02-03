#pragma once

#include <string>
#include <map>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/plugin.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Base class for a typical implementation of the metadata plugin. Hides many technical details of
 * the Metadata Plugin SDK, but may limit plugin capabilities - use only when suitable.
 */
class CommonPlugin: public nxpt::CommonRefCounter<Plugin>
{
protected:
    CommonPlugin(const char* name);

    virtual std::string capabilitiesManifest() const = 0;

    /**
     * Provides access to the Plugin global settings stored by the server.
     * @return Param value, or an empty string if such param does not exist, having logged the
     * error.
     */
    std::string getParamValue(const char* paramName);

public:
    virtual ~CommonPlugin() override;

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual const char* name() const override;
    virtual void setSettings(const nxpl::Setting* settings, int count) override;
    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;
    virtual void setLocale(const char* locale) override;

    virtual const char* capabilitiesManifest(Error* error) const override;

private:
    const char* const m_name;
    std::map<std::string, std::string> m_settings;
    mutable std::string m_manifest; //< Cache the manifest to guarantee a lifetime for char*.
};

} // namespace metadata
} // namespace sdk
} // namespace nx
