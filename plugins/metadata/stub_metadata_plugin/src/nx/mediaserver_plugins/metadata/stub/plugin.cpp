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
    return R"json(
        {
            "driverId": ")json" + kDriverGuid + R"json(",
            "driverName": {
                "value": "Stub Driver"
            },
            "outputEventTypes": [
                {
                    "typeId": ")json" + kLineCrossingEventGuid + R"json(",
                    "name": {
                        "value": "Line crossing"
                    }
                },
                {
                    "typeId": ")json" + kObjectInTheAreaEventGuid + R"json(",
                    "name": {
                        "value": "Object in the area"
                    },
                    "flags": "stateDependent|regionDependent"
                }
            ],
            "outputObjectTypes": [
                {
                    "typeId": ")json" + kCarObjectGuid + R"json(",
                    "name": {
                        "value": "Car"
                    }
                },
                {
                    "typeId": ")json" + kHumanFaceObjectGuid + R"json(",
                    "name": {
                        "value": "Human face"
                    }
                }
            ],
            "capabilities": "needDeepCopyForMediaFrame|needUncompressedVideoFrames",
            "settings": {
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
                        "id": "groupA",
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
            },
            "objectActions": [
                {
                    "id": "nx.stub.addToList",
                    "name": {
                        "value": "Add to list"
                    },
                    "supportedObjectTypeIds": [
                        ")json" + kCarObjectGuid + R"json("
                    ],
                    "settings": {
                        "params": [
                            {
                                "id": "paramA",
                                "dataType": "Number",
                                "name": "Param A",
                                "description": "Number A"
                            },
                            {
                                "id": "paramB",
                                "dataType": "Enumeration",
                                "range": "b1,b3",
                                "name": "Param B",
                                "description": "Enumeration B"
                            }
                        ]
                    }
                },
                {
                    "id": "nx.stub.addPerson",
                    "name": {
                        "value": "Add person (URL-based)"
                    },
                    "supportedObjectTypeIds": [
                        ")json" + kCarObjectGuid + R"json("
                    ]
                }
            ]
        }
    )json";
}

void Plugin::settingsChanged()
{
    NX_PRINT << __func__ << "()";
}

void Plugin::executeAction(
    const std::string& actionId,
    nxpl::NX_GUID objectId,
    const std::map<std::string, std::string>& params,
    std::string* outActionUrl,
    std::string* outMessageToUser,
    Error* error)
{
    if (actionId == "nx.stub.addToList")
    {
        NX_PRINT << __func__
            << "(): id [nx.stub.addToList]; returning a message with param values.";

        std::string valueA;
        auto paramAIt = params.find("paramA");
        if (paramAIt != params.cend())
            valueA = paramAIt->second;

        std::string valueB;
        auto paramBIt = params.find("paramB");
        if (paramBIt != params.cend())
            valueB = paramBIt->second;

        *outMessageToUser = std::string("Your param values are: ")
            + "paramA: [" + valueA + "], "
            + "paramB: [" + valueB + "]";

    }
    else if (actionId == "nx.stub.addPerson")
    {
        *outActionUrl = "http://internal.server/addPerson?objectId=" +
            nxpt::NxGuidHelper::toStdString(objectId);
        NX_PRINT << __func__
            << "(): id [nx.stub.addPerson]; returning URL: [" << *outActionUrl << "]";
    }
    else
    {
        NX_PRINT << __func__ << "(): ERROR: Unsupported action id: [" << actionId << "]";
        *error = Error::unknownError;
    }
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
