// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"
#include "stub_analytics_plugin_taxonomy_features_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace taxonomy_features {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(ini().enableOutput, plugin->instanceId()),
    m_plugin(plugin)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

std::string Engine::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*) R"json(
{
    "streamTypeFilter": "compressedVideo",
    "typeLibrary":
    {
        "objectTypes":
        [
            {
                "id": "nx.stub.objectTypeFromEngineManifest",
                "name": "Stub: Object Type from Engine Manifest",
                "attributes":
                [
                    {
                        "type": "String",
                        "name": "String attribute (Engine)"
                    },
                    {
                        "type": "Enum",
                        "subtype": "nx.stub.enumTypeFromEngineManifest",
                        "name": "Enum attribute (Engine)"
                    },
                    {
                        "type": "Color",
                        "subtype": "nx.stub.colorTypeFromEngineManifest",
                        "name": "Color attribute (Engine)"
                    },
                    {
                        "type": "Object",
                        "subtype": "nx.base.Bag",
                        "name": "Object attribute using Object Type from Base Type Library (Engine)"
                    }
                ]
            }
        ],
        "enumTypes":
        [
            {
                "id": "nx.stub.enumTypeFromEngineManifest",
                "name": "Stub: Enum Type from Engine manifest",
                "items":
                [
                    "Engine item 1",
                    "Engine item 2",
                    "Engine item 3"
                ]
            }
        ],
        "colorTypes":
        [
            {
                "id": "nx.stub.colorTypeFromEngineManifest",
                "name": "Stub: Color Type from Engine manifest",
                "items":
                [
                    {
                        "name": "red",
                        "rgb": "#FF0000"
                    },
                    {
                        "name": "green",
                        "rgb": "#00FF00"
                    },
                    {
                        "name": "blue",
                        "rgb": "#0000FF"
                    }
                ]
            }
        ]
    },
    "deviceAgentSettingsModel":
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "CheckBox",
                "caption": "Base Object Type",
                "name": "generateInstanceOfBaseObjectType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Derived Object Type",
                "name": "generateInstanceOfDerivedObjectType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Derived Object Type with own attributes",
                "name": "generateInstanceOfDerivedObjectTypeWithOmittedAttributes",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Hidden derived Object Type",
                "name": "generateInstanceOfHiddenDerivedObjectType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Hidden derived Object Type with own attributes",
                "name": "generateInstanceOfHiddenDerivedObjectTypeWithOwnAttributes",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Derived Object Type with unsupported base",
                "name": "generateInstanceOfDerivedObjectTypeWithUnsupportedBase",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Object Type with numeric attributes",
                "name": "generateInstanceOfObjectTypeWithNumericAttibutes",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Object Type with boolean attributes",
                "name": "generateInstanceOfObjectTypeWithBooleanAttibutes",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Object Type with icon",
                "name": "generateInstanceOfObjectTypeWithIcon",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Object Type inherited from the Base Type Library type",
                "name": "generateInstanceOfObjectTypeInheritedFromBaseTypeLibraryType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Object Type with Base Type Library Enum attribute",
                "name": "generateInstanceOfObjectTypeUsingBaseTypeLibraryEnumType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Object Type with Base Type Library Color attribute",
                "name": "generateInstanceOfObjectTypeUsingBaseTypeLibraryColorType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Object Type with Base Type Library Object attribute",
                "name": "generateInstanceOfObjectTypeUsingBaseTypeLibraryObjectType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Base Type Library Object Type",
                "name": "generateInstanceOfOfBaseTypeLibraryObjectType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Object Type declared in the Engine manifest",
                "name": "generateInstanceOfObjectTypeDeclaredInEngineManifest",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Live-only Object Type",
                "name": "generateInstanceOfLiveOnlyObjectType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Non-indexable Object Type",
                "name": "generateInstanceOfNonIndexableObjectType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Extended Object Type",
                "name": "generateInstanceOfExtendedObjectType",
                "defaultValue": true
            },
            {
                "type": "CheckBox",
                "caption": "Object Type with Attribute List",
                "name": "generateInstanceOfObjectTypeWithAttributeList",
                "defaultValue": true
            }
        ]
    }
}
)json";
}

} // namespace taxonomy_features
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
