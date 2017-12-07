#include "plugin.h"

#include <nx/kit/debug.h>

#include <plugins/plugin_tools.h>

#include "manager.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace tegra_video {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin()
{
    NX_PRINT << "Created \"" << name() << "\"";
}

Plugin::~Plugin()
{
    NX_PRINT << "Destroyed \"" << name() << "\"";
}

void* Plugin::queryInterface(const nxpl::NX_GUID& interfaceId)
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

const char* Plugin::name() const
{
    return "Tegra Video metadata plugin";
}

void Plugin::setSettings(const nxpl::Setting* /*settings*/, int /*count*/)
{
    // Do nothing.
}

void Plugin::setPluginContainer(nxpl::PluginInterface* /*pluginContainer*/)
{
    // Do nothing.
}

void Plugin::setLocale(const char* /*locale*/)
{
    // Do nothing.
}

AbstractMetadataManager* Plugin::managerForResource(
    const ResourceInfo& resourceInfo,
    Error* outError)
{
    *outError = Error::noError;

    auto manager = new Manager(this);
    manager->addRef();

    return manager;
}

AbstractSerializer* Plugin::serializerForType(
    const nxpl::NX_GUID& /*typeGuid*/,
    Error* /*outError*/)
{
    return nullptr;
}

const char* Plugin::capabilitiesManifest(Error* error) const
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
            "outputEventTypes": [
                {
                    "typeId": "{7E94CE15-3B69-4719-8DFD-AC1B76E5D8F4}",
                    "name": {
                        "value": "Human entered the area.",
                        "localization": {
                            "ru_RU": "Human leaved the area (translated to Russian)"
                        }
                    }
                },
                {
                    "typeId": "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}",
                    "name": {
                        "value": "Human leaved the area",
                        "localization": {
                            "ru_RU": "Human leaved the area (translated to Russian)"
                        }
                    }
                }
            ],
            "outputObjectTypes": [
                {
                    "typeId": "{58AE392F-8516-4B27-AEE1-311139B5A37A}",
                    "name": {
                        "value": "Car",
                        "localization": {
                            "ru_RU": "Car (translated to Russian)"
                        }
                    }
                },
                {
                    "typeId": "{3778A599-FB60-47E9-8EC6-A9949E8E0AE7}",
                    "name": {
                        "value": "Human",
                        "localization": {
                            "ru_RU": "Human (translated to Russian)"
                        }
                    }
                }
            ],


            "options": "needDeepCopyForMediaFrame"
        }
    )json";
}

} // namespace tegra_video
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver_plugins::metadata::tegra_video::Plugin();
}

} // extern "C"
