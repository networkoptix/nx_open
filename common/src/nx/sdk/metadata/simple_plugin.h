#pragma once

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
public:
    SimplePlugin(const char* name);
    virtual ~SimplePlugin() override;

    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;
    virtual const char* name() const override;
    virtual void setSettings( const nxpl::Setting* settings, int count ) override;
    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;
    virtual void setLocale(const char* locale) override;

    virtual nx::sdk::metadata::AbstractSerializer* serializerForType(
        const nxpl::NX_GUID& typeGuid,
        nx::sdk::Error* outError) override;

private:
    const char* const m_name;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
