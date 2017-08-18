#pragma once

#include <plugins/plugin_tools.h>
#include <nx/sdk/metadata/abstract_metadata_plugin.h>

namespace nx {
namespace sdk {
namespace metadata {

class StubMetadataPlugin: public nxpt::CommonRefCounter<AbstractMetadataPlugin>
{
public:
    virtual void* queryInterface(const nxpl::NX_GUID& interfaceId) override;

    virtual const char* name() const override;
    
    virtual void setSettings( const nxpl::Setting* settings, int count ) override;

    virtual void setPluginContainer(nxpl::PluginInterface* pluginContainer) override;

    virtual void setLocale(const char* locale) override;

    virtual AbstractMetadataManager* managerForResource(
        const ResourceInfo& resourceInfo,
        Error* outError) override;

    virtual AbstractSerializer* serializerForType(
        const nxpl::NX_GUID& typeGuid,
        Error* outError) override;

    virtual const char* capabilitiesManifest(Error* error) const override;
};

} // namespace metadata
} // namespace sdk
} // namespace nx
