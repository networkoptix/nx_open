#include "plugin.h"

#define NX_DEBUG_ENABLE_OUTPUT true //< Stub plugin is itself a debug feature, thus is verbose.
#include <nx/kit/debug.h>

#include "camera_manager.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin(): CommonPlugin("Stub metadata plugin")
{
    setEnableOutput(NX_DEBUG_ENABLE_OUTPUT); //< Base class is verbose when this descendant is.
}

nx::sdk::metadata::CameraManager* Plugin::obtainCameraManager(
    const CameraInfo& /*cameraInfo*/, Error* /*outError*/)
{
    return new CameraManager(this);
}

std::string Plugin::capabilitiesManifest() const
{
    static const std::string kLineCrossingEventGuidStr = nxpt::NxGuidHelper::toStdString(
        kLineCrossingEventGuid);
    static const std::string kObjectInTheAreaEventGuidStr = nxpt::NxGuidHelper::toStdString(
        kObjectInTheAreaEventGuid);
    static const std::string kCarObjectGuidStr = nxpt::NxGuidHelper::toStdString(
        kCarObjectGuid);

    return R"json(
        {
            "driverId": "{B14A8D7B-8009-4D38-A60D-04139345432E}",
            "driverName": {
                "value": "Stub Driver"
            },
            "outputEventTypes": [
                {
                    "typeId": ")json" + kLineCrossingEventGuidStr + R"json(",
                    "name":
                        "value": "Line crossing"
                    }
                },
                {
                    "typeId": ")json" + kObjectInTheAreaEventGuidStr + R"json(",
                    "name": {
                        "value": "Object in the area"
                    },
                    "flags": "stateDependent|regionDependent"
                }
            ],
            "outputObjectTypes": [
                {
                    "typeId": ")json" + kCarObjectGuidStr + R"json(",
                    "name": {
                        "value": "Car"
                    }
                },
                {
                    "typeId": "{C23DEF4D-04F7-4B4C-994E-0C0E6E8B12CB}",
                    "name": {
                        "value": "Human face detected"
                    }
                }
            ],
            "capabilities": "needDeepCopyForMediaFrame",
            "parameters": {
                "groups": [
                    {
                        "params": [
                            {
                                "id": "paramAId",
                                "dataType": "Number",
                                "name": "Param A",
                                "description": "Number A"
                            },
                            {
                                "id": "paramBId",
                                "dataType": "Enumeration",
                                "range": "b1,b3",
                                "name": "Param B",
                                "description": "Enumeration B"
                            }
                        ],
                        "groups": [
                            {
                                "id": "groupA"
                                "name": "Group name",
                                "params": [
                                    {
                                        "id": "groupA.paramA",
                                        "dataType": "String",
                                        "name": "Some string",
                                        "description": "Some string param in a group"
                                    }
                                ],
                                "groups": [
                                ]
                            }
                        ]
                    }
                ]
            }
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
