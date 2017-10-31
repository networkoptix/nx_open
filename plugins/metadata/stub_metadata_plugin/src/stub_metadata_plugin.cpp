#include "stub_metadata_plugin.h"
#include "stub_metadata_manager.h"

namespace nx {
namespace mediaserver {
namespace plugins {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

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

const char* StubMetadataPlugin::capabilitiesManifest(Error* error) const
{
    *error = Error::noError;

    return R"manifest(
    {
        "driverId": "{B14A8D7B-8009-4D38-A60D-04139345432E}",
        "driverName": {
            "value": "Stub Driver",
            "localization": {
                "ru_RU": "Stub driver (translated to Russian)"
            }
        },
        "outputEventTypes": [
            {
                "eventTypeId": "{7E94CE15-3B69-4719-8DFD-AC1B76E5D8F4}",
                "eventName": {
                    "value": "Line crossing",
                    "localization": {
                        "ru_RU": "Line crossing (translated to Russian)"
                    }
                }
            },
            {
                "eventTypeId": "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}",
                "eventName": {
                    "value": "Object in the area",
                    "localization": {
                        "ru_RU": "Object in the area (translated to Russian)"
                    }
                }
            }
        ],
        "options": "needDeepCopyForMediaFrame"
    }
    )manifest";
}

} // namespace plugins
} // namespace mediaserver
} // namespace nx

extern "C" {

    NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
    {
        return new nx::mediaserver::plugins::StubMetadataPlugin();
    }

} // extern "C"