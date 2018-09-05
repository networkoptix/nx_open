#include "plugin.h"

#define NX_PRINT_PREFIX (this->utils.printPrefix)
#include <nx/kit/debug.h>

#include "camera_manager.h"
#include "stub_metadata_plugin_ini.h"

namespace nx {
namespace mediaserver_plugins {
namespace metadata {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::metadata;

Plugin::Plugin()
    : CommonPlugin("Stub metadata plugin", "stub_metadata_plugin", NX_DEBUG_ENABLE_OUTPUT)
{
    initCapabilities();
}

nx::sdk::metadata::CameraManager* Plugin::obtainCameraManager(
    const CameraInfo* /*cameraInfo*/, Error* /*outError*/)
{
    return new CameraManager(this);
}

void Plugin::initCapabilities()
{
    const std::string pixelFormatString = ini().needUncompressedVideoFrames;
    if (!pixelFormatString.empty())
    {
        if (!pixelFormatFromStdString(pixelFormatString, &m_pixelFormat))
        {
            NX_PRINT << "ERROR: Invalid value of needUncompressedVideoFrames in "
                     << ini().iniFile() << ": [" << pixelFormatString << "].";
        }
        else
        {
            m_needUncompressedVideoFrames = true;
            m_capabilities += std::string("|needUncompressedVideoFrames_") + pixelFormatString;
        }
    }

    // Delete first '|', if any.
    if (!m_capabilities.empty() && m_capabilities.at(0) == '|')
        m_capabilities.erase(0, 1);
}

std::string Plugin::capabilitiesManifest() const
{
    return R"json(
        {
            "pluginId": "nx.stub",
            "pluginName": {
                "value": "Stub Metadata Plugin"
            },
            "outputEventTypes": [
                {
                    "id": ")json" + kLineCrossingEventType + R"json(",
                    "name": {
                        "value": "Line crossing"
                    }
                },
                {
                    "id": ")json" + kObjectInTheAreaEventType + R"json(",
                    "name": {
                        "value": "Object in the area"
                    },
                    "flags": "stateDependent|regionDependent"
                }
            ],
            "outputObjectTypes": [
                {
                    "id": ")json" + kCarObjectType+ R"json(",
                    "name": {
                        "value": "Car"
                    }
                },
                {
                    "id": ")json" + kHumanFaceObjectType+ R"json(",
                    "name": {
                        "value": "Human face"
                    }
                }
            ],
            "capabilities": ")json" +
        m_capabilities + R"json(",
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
                        "nx.stub.car"
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
                        "nx.stub.car"
                    ]
                }
            ]
        }
    )json";
}

void Plugin::settingsChanged() { NX_PRINT << __func__ << "()"; }

void Plugin::executeAction(const std::string& actionId,
    nxpl::NX_GUID objectId,
    nxpl::NX_GUID /*cameraId*/,
    int64_t /*timestampUs*/,
    const std::map<std::string, std::string>& params,
    std::string* outActionUrl,
    std::string* outMessageToUser,
    Error* error)
{
    if (actionId == "nx.stub.addToList")
    {
        std::string valueA;
        auto paramAIt = params.find("paramA");
        if (paramAIt != params.cend())
            valueA = paramAIt->second;

        std::string valueB;
        auto paramBIt = params.find("paramB");
        if (paramBIt != params.cend())
            valueB = paramBIt->second;

        *outMessageToUser = std::string("Your param values are: ") + "paramA: [" + valueA + "], " +
            "paramB: [" + valueB + "]";

        NX_PRINT << __func__ << "(): Returning a message: [" << *outMessageToUser << "]";
    }
    else if (actionId == "nx.stub.addPerson")
    {
        *outActionUrl = "http://internal.server/addPerson?objectId=" + nxpt::toStdString(objectId);
        NX_PRINT << __func__ << "(): Returning URL: [" << *outActionUrl << "]";
    }
    else
    {
        NX_PRINT << __func__ << "(): ERROR: Unsupported actionId.";
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
