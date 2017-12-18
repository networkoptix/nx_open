#include "plugin.h"

#include <nx/kit/debug.h>

#include "manager.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

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
    return "Stub metadata plugin";
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
    const ResourceInfo& /*resourceInfo*/,
    Error* outError)
{
    *outError = Error::noError;
    return new Manager(this);
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
                "value": "Stub Driver",
                "localization": {
                    "ru_RU": "Stub driver (translated to Russian)"
                }
            },
            "outputEventTypes": [
                {
                    "typeId": "{7E94CE15-3B69-4719-8DFD-AC1B76E5D8F4}",
                    "name": {
                        "value": "Line crossing",
                        "localization": {
                            "ru_RU": "Line crossing (translated to Russian)"
                        }
                    }
                },
                {
                    "typeId": "{B0E64044-FFA3-4B7F-807A-060C1FE5A04C}",
                    "name": {
                        "value": "Object in the area",
                        "localization": {
                            "ru_RU": "Object in the area (translated to Russian)"
                        }
                    }
                }
            ],
            "outputObjectTypes": [
                {
                    "typeId": "{153DD879-1CD2-46B7-ADD6-7C6B48EAC1FC}",
                    "name": {
                        "value": "Car detected",
                        "localization": {
                            "ru_RU": "Car detected (translated to Russian)"
                        }
                    }
                },
                {
                    "typeId": "{C23DEF4D-04F7-4B4C-994E-0C0E6E8B12CB}",
                    "name": {
                        "value": "Human face detected",
                        "localization": {
                            "ru_RU": "Human face detected (translated to Russian)"
                        }
                    }
                }
            ],
            "capabilities": "needDeepCopyForMediaFrame"
        }
    )json";
}

} // namespace stub
} // namespace metadata
} // namespace mediaserver_plugins
} // namespace nx

extern "C" {

NX_PLUGIN_API nxpl::PluginInterface* createNxMetadataPlugin()
{
    return new nx::mediaserver_plugins::metadata::stub::Plugin();
}

} // extern "C"
