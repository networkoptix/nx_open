#include "stub_metadata_plugin.h"
#include "stub_metadata_manager.h"

namespace nx {
namespace sdk {
namespace metadata {

void* StubMetadataPlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
{
    if (interfaceId == IID_MetadataPlugin)
    {
        addRef();
        return static_cast<AbstractMetadataPlugin*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin3)
    {
        addRef();
        return static_cast<nxpl::Plugin3*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin2)
    {
        addRef();
        return static_cast<nxpl::Plugin2*>(this);
    }

    if (interfaceId == nxpl::IID_Plugin)
    {
        addRef();
        return static_cast<nxpl::Plugin*>(this);
    }

    if (interfaceId == nxpl::IID_PluginInterface)
    {
        addRef();
        return static_cast<nxpl::PluginInterface*>(this);
    }
    return nullptr;
}

const char* StubMetadataPlugin::name() const
{
    return "Stub metadata plugin";
}

void StubMetadataPlugin::setSettings(const nxpl::Setting* settings, int count)
{
    // Do nothing.
}

void StubMetadataPlugin::setPluginContainer(nxpl::PluginInterface* pluginContainer)
{
    // Do nothing.
}

void StubMetadataPlugin::setLocale(const char* locale)
{
    // Do nothing.
}

AbstractMetadataManager* StubMetadataPlugin::managerForResource(
    const ResourceInfo& resourceInfo,
    Error* outError)
{
    *outError = Error::noError;

    auto manager = new StubMetadataManager();
    manager->addRef();

    return manager;
}

AbstractSerializer* StubMetadataPlugin::serializerForType(
    const nxpl::NX_GUID& typeGuid,
    Error* outError)
{
    return nullptr;
}

} // namespace metadata
} // namespace sdk
} // namespace nx

extern "C" {

    NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
    {
        return new nx::sdk::metadata::StubMetadataPlugin();
    }

} // extern "C"