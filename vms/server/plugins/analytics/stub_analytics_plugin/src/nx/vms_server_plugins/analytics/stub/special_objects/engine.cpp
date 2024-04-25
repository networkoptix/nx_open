// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>
#include <nx/sdk/helpers/settings_response.h>

#include "../utils.h"

#include "device_agent.h"
#include "settings_model.h"
#include "stub_analytics_plugin_special_objects_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace special_objects {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using namespace std::chrono;
using namespace std::literals::chrono_literals;
using Uuid = nx::sdk::Uuid;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT, plugin->instanceId()),
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

static std::string buildCapabilities()
{
    std::string capabilities;

    if (ini().deviceDependent)
        capabilities += "|deviceDependent";

    if (ini().keepObjectBoundingBoxRotation)
        capabilities += "|keepObjectBoundingBoxRotation";

    // Delete first '|', if any.
    if (!capabilities.empty() && capabilities.at(0) == '|')
        capabilities.erase(0, 1);

    return capabilities;
}

std::string Engine::manifestString() const
{
    std::string result = /*suppress newline*/ 1 + (const char*) R"json(
{
    "typeLibrary":
    {
        "objectTypes":
        [
            {
                "id": ")json" + kFixedObjectType + R"json(",
                "name": "Fixed object"
            }
        ]
    },
    "capabilities": ")json" + buildCapabilities() + R"json(",
    "streamTypeFilter": "compressedVideo",
    "objectActions":
    [
        {
            "id": "nx.stub.addToList",
            "name": "Add to list",
            "supportedObjectTypeIds":
            [
                ")json" + kFixedObjectType + R"json("
            ],
            "requirements":
            {
                "capabilities": "needBestShotVideoFrame|needBestShotObjectMetadata|needFullTrack",
                "bestShotVideoFramePixelFormat": "yuv420"
            },
            "parametersModel":
            {
                "type": "Settings",
                "items":
                [
                    {
                        "type": "TextField",
                        "name": "testTextField",
                        "caption": "Text Field Parameter",
                        "description": "A text field",
                        "defaultValue": "a text"
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Parameter Group",
                        "items":
                        [
                            {
                                "type": "SpinBox",
                                "caption": "SpinBox Parameter",
                                "name": "testSpinBox",
                                "defaultValue": 42,
                                "minValue": 0,
                                "maxValue": 100
                            },
                            {
                                "type": "DoubleSpinBox",
                                "caption": "DoubleSpinBox Parameter",
                                "name": "testDoubleSpinBox",
                                "defaultValue": 3.1415,
                                "minValue": 0.0,
                                "maxValue": 100.0
                            },
                            {
                                "type": "ComboBox",
                                "name": "testComboBox",
                                "caption": "ComboBox Parameter",
                                "defaultValue": "value2",
                                "range": ["value1", "value2", "value3"]
                            },
                            {
                                "type": "CheckBox",
                                "caption": "CheckBox Parameter",
                                "name": "testCheckBox",
                                "defaultValue": true
                            }
                        ]
                    }
                ]
            }
        },
        {
            "id": "nx.stub.addPerson",
            "name": "Add person (URL-based)",
            "supportedObjectTypeIds":
            [
                ")json" + kFixedObjectType + R"json("
            ]
        }
    ],
    "deviceAgentSettingsModel":
)json"
        + kSettingsModel
        + R"json(
}
)json";

    return result;
}

static std::string timestampedObjectMetadataToString(
    Ptr<const ITimestampedObjectMetadata> metadata)
{
    if (!metadata)
        return "null";

    std::string attributeString;
    if (metadata->attributeCount() > 0)
    {
        attributeString += "\n    Attributes:\n";
        for (int i = 0; i < metadata->attributeCount(); ++i)
        {
            const Ptr<const IAttribute> attribute = metadata->attribute(i);
            if (!attribute)
            {
                attributeString += "null";
            }
            else
            {
                attributeString += "        "
                    + nx::kit::utils::toString(attribute->name())
                    + ": "
                    + nx::kit::utils::toString(attribute->value());
            }

            if (i < metadata->attributeCount() - 1)
                attributeString += "\n";
        }
    }

    return nx::kit::utils::format("timestamp: %lld, id: %s",
        metadata->timestampUs(), UuidHelper::toStdString(metadata->trackId()).c_str())
        + attributeString;
}

static std::string uncompressedVideoFrameToString(Ptr<const IUncompressedVideoFrame> frame)
{
    if (!frame)
        return "null";

    return nx::kit::utils::format("%dx%d %s",
        frame->width(), frame->height(), pixelFormatToStdString(frame->pixelFormat()).c_str());
}

Result<IAction::Result> Engine::executeAction(
    const std::string& actionId,
    Uuid trackId,
    Uuid /*deviceId*/,
    int64_t /*timestampUs*/,
    nx::sdk::Ptr<IObjectTrackInfo> objectTrackInfo,
    const std::map<std::string, std::string>& params)
{
    std::string messageToUser;
    std::string actionUrl;

    if (actionId == "nx.stub.addToList")
    {
        messageToUser = std::string("Track id: ") + UuidHelper::toStdString(trackId) + "\n\n";

        if (!objectTrackInfo)
        {
            messageToUser += "No object track info provided.\n\n";
        }
        else
        {
            messageToUser += std::string("Object track info:\n")
                + "    Best shot frame: "
                    + uncompressedVideoFrameToString(objectTrackInfo->bestShotVideoFrame()) + "\n"
                + "    Best shot metadata: "
                    + timestampedObjectMetadataToString(objectTrackInfo->bestShotObjectMetadata())
                + "\n\n";
        }

        if (!params.empty())
        {
            messageToUser += std::string("Your param values are:\n");
            bool first = true;
            for (const auto& entry: params)
            {
                const auto& parameterName = entry.first;
                const auto& parameterValue = entry.second;

                if (!first)
                    messageToUser += ",\n";
                else
                    first = false;
                messageToUser += parameterName + ": [" + parameterValue + "]";
            }
        }
        else
        {
            messageToUser += "No param values provided.";
        }

        NX_PRINT << __func__ << "(): Returning a message: "
            << nx::kit::utils::toString(messageToUser);
    }
    else if (actionId == "nx.stub.addPerson")
    {
        actionUrl = ini().addPersonActionUrlPrefix + UuidHelper::toStdString(trackId);
        NX_PRINT << __func__ << "(): Returning URL: " << nx::kit::utils::toString(actionUrl);
    }
    else
    {
        NX_PRINT << __func__ << "(): ERROR: Unsupported actionId.";
        return error(ErrorCode::invalidParams, "Unsupported actionId");
    }

    return IAction::Result{makePtr<String>(actionUrl), makePtr<String>(messageToUser)};
}

} // namespace special_objects
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
