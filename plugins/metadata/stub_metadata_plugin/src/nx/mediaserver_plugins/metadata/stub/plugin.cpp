#include "plugin.h"

#define NX_PRINT_PREFIX printPrefix()
#include <nx/kit/debug.h>

#include "camera_manager.h"
#include "stub_metadata_plugin_ini.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin():
    CommonPlugin("Stub metadata plugin", "stub_metadata_plugin", NX_DEBUG_ENABLE_OUTPUT)
{
    initCapabilities();
}

nx::sdk::metadata::CameraManager* Plugin::obtainCameraManager(
    const CameraInfo& /*cameraInfo*/, Error* /*outError*/)
{
    return new CameraManager(this);
}

void Plugin::initCapabilities()
{
    if (ini().needDeepCopyOfVideoFrames)
    {
        m_needDeepCopyOfVideoFrames = true;
        m_capabilities += "|needDeepCopyOfVideoFrames";
    }

    if (ini().needUncompressedVideoFrames)
    {
        m_needUncompressedVideoFrames = true;
        m_capabilities += "|needUncompressedVideoFrames";
    }

    // Delete first '|', if any.
    if (!m_capabilities.empty() && m_capabilities.at(0) == '|')
        m_capabilities.erase(0, 1);
}

std::string Plugin::capabilitiesManifest() const
{
    using Guid = nxpt::NxGuidHelper;
    return R"json(
        {
            "driverId": ")json" + Guid::toStdString(kDriverGuid) + R"json(",
            "driverName": {
                "value": "Stub Driver"
            },
            "outputEventTypes": [
                {
                    "typeId": ")json" + Guid::toStdString(kLineCrossingEventGuid) + R"json(",
                    "name": {
                        "value": "Line crossing"
                    }
                },
                {
                    "typeId": ")json" + Guid::toStdString(kObjectInTheAreaEventGuid) + R"json(",
                    "name": {
                        "value": "Object in the area"
                    },
                    "flags": "stateDependent|regionDependent"
                }
            ],
            "outputObjectTypes": [
                {
                    "typeId": ")json" + Guid::toStdString(kCarObjectGuid) + R"json(",
                    "name": {
                        "value": "Car"
                    }
                },
                {
                    "typeId": ")json" + Guid::toStdString(kHumanFaceObjectGuid) + R"json(",
                    "name": {
                        "value": "Human face"
                    }
                }
            ],
            "capabilities": ")json" + m_capabilities + R"json(",
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
                        ")json" + nxpt::NxGuidHelper::toStdString(kCarObjectGuid) + R"json("
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
                        ")json" + Guid::toStdString(kCarObjectGuid) + R"json("
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
    nxpl::NX_GUID cameraId,
    int64_t timestampUs,
    const std::map<std::string, std::string>& params,
    std::string* outActionUrl,
    std::string* outMessageToUser,
    Error* error)
{
    const std::string logHeader = std::string(__func__)
        + "(actionId: [" + actionId + "]"
        + ", objectId: " + nxpt::NxGuidHelper::toStdString(objectId)
        + ", cameraId: " + nxpt::NxGuidHelper::toStdString(cameraId)
        + ", timestampUs: " + std::to_string(timestampUs)
        + ")";

    if (actionId == "nx.stub.addToList")
    {
        NX_PRINT << logHeader << ": Returning a message with param values.";

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
        NX_PRINT << logHeader << ": Returning URL: [" << *outActionUrl << "]";
    }
    else
    {
        NX_PRINT << logHeader << ": ERROR: Unsupported actionId.";
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
