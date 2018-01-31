#pragma once

#include <map>

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>

namespace nx {
namespace sdk {
namespace metadata {

/**
 * Base class for a typical implementation of the metadata plugin. Hides many technical details of
 * the Metadata Plugin SDK, but may limit plugin capabilities - use only when suitable.
 */
class SimplePlugin: public nxpt::CommonRefCounter<AbstractMetadataPlugin>
{
protected:
    SimplePlugin(const char* name);

    /**
     * Provides access to the Plugin global settings stored by the server.
     * @return Param value, or an empty string if such param does not exist, having logged the
     * error.
     */
    std::string getParamValue(const char* paramName);

public:
    virtual ~SimplePlugin() override;

//-------------------------------------------------------------------------------------------------
// Not intended to be used by a descendant.

public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual const char* name() const override;
    virtual void setSettings(const nxpl::Setting* settings, int count) override;
    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;
    virtual void setLocale(const char* locale) override;

    virtual AbstractSerializer* serializerForType(
        const nxpl::NX_GUID& typeGuid,
        Error* outError) override;

private:
    const char* const m_name;
    std::map<std::string, std::string> m_settings;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
