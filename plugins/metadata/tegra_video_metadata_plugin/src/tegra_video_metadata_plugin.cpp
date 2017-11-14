#include "tegra_video_metadata_plugin.h"
#include "tegra_video_metadata_manager.h"

namespace nx {
namespace mediaserver {
namespace plugins {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

void* TegraVideoMetadataPlugin::queryInterface(const nxpl::NX_GUID& interfaceId)
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

const char* TegraVideoMetadataPlugin::name() const
{
    return "Tegra Video metadata plugin";
}

void TegraVideoMetadataPlugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Do nothing.
}

void TegraVideoMetadataPlugin::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
    // Do nothing.
}

void TegraVideoMetadataPlugin::setLocale(const char* /*locale*/)
{
    // Do nothing.
}

AbstractMetadataManager* TegraVideoMetadataPlugin::managerForResource(
    const ResourceInfo& resourceInfo,
    Error* outError)
{
    *outError = Error::noError;

    auto manager = new TegraVideoMetadataManager();
    manager->addRef();

    return manager;
}

AbstractSerializer* TegraVideoMetadataPlugin::serializerForType(
    const nxpl::NX_GUID& /*typeGuid*/,
    Error* /*outError*/)
{
    return nullptr;
}

const char* TegraVideoMetadataPlugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;

    return R"json(
    {
        "driverId": "{B14A8D7B-8009-4D38-A60D-04139345432E}",
        "driverName": {
            "value": "Tegra Video Driver",
            "localization": {
                "ru_RU": "Tegra Video driver (translated to Russian)"
            }
        },
        "options": "needDeepCopyForMediaFrame"
    }
    )json";
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver::plugins::TegraVideoMetadataPlugin();
}

} // extern "C"
