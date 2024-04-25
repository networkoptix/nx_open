// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#include "device_agent.h"
#include "common.h"

#include <nx/kit/utils.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/analytics/helpers/pixel_format.h>

#include "stub_analytics_plugin_object_actions_ini.h"

#undef NX_PRINT_PREFIX
#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace object_actions {

using namespace nx::sdk;
using namespace nx::sdk::analytics;
using Uuid = nx::sdk::Uuid;

static std::string parametersToString(const std::map<std::string, std::string>& parameters)
{
    std::string result;

    for (const auto& entry: parameters)
    {
        const std::string& parameterName = entry.first;
        const std::string& parameterValue = entry.second;

        result += parameterName + ":" + parameterValue + ", ";
    }

    if (!parameters.empty())
        result = result.substr(0, result.size() - 2);

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
        attributeString += "\n            Attributes:\n";
        for (int i = 0; i < metadata->attributeCount(); ++i)
        {
            const Ptr<const IAttribute> attribute = metadata->attribute(i);
            if (!attribute)
            {
                attributeString += "null";
            }
            else
            {
                attributeString += "                "
                    + nx::kit::utils::toString(attribute->name())
                    + ": "
                    + nx::kit::utils::toString(attribute->value());
            }

            if (i < metadata->attributeCount() - 1)
                attributeString += "\n";
        }
    }

    return nx::kit::utils::format("Timestamp: %lld us", metadata->timestampUs())
        + attributeString;
}

static std::string uncompressedVideoFrameToString(Ptr<const IUncompressedVideoFrame> frame)
{
    if (!frame)
        return "null";

    return nx::kit::utils::format("%dx%d %s",
        frame->width(), frame->height(), pixelFormatToStdString(frame->pixelFormat()).c_str());
}

Engine::Engine(): nx::sdk::analytics::Engine(ini().enableOutput)
{
}

Engine::~Engine()
{
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(deviceInfo);
}

std::string Engine::manifestString() const
{
    return /*suppress newline*/ 1 + (const char*)
R"json(
{
    "streamTypeFilter": "compressedVideo",
    "objectActions":
    [
        {
            "id": ")json" + kObjectActionWithMessageResultId + R"json(",
            "name": "Stub: Object Action with message result",
            "supportedObjectTypeIds": [ ")json" + kObjectTypeId + R"json(" ]
        },
        {
            "id": ")json" + kObjectActionWithUrlResultId + R"json(",
            "name": "Stub: Object Action with URL result",
            "supportedObjectTypeIds": [ ")json" + kObjectTypeId + R"json(" ]
        },
        {
            "id": ")json" + kObjectActionWithParametersId + R"json(",
            "name": "Stub: Object Action with parameters",
            "supportedObjectTypeIds": [ ")json" + kObjectTypeId + R"json(" ],
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
                                "caption": "ComboBox Parameter",
                                "name": "testComboBox",
                                "defaultValue": "value2",
                                "range": ["value1", "value2", "value3"]
                            },
                            {
                                "type": "CheckBox",
                                "caption": "CheckBox Parameter",
                                "name": "testCheckBox",
                                "defaultValue": true
                            },
                            {
                                "type": "TextArea",
                                "caption": "TextArea Parameter",
                                "name": "testTextArea"
                            }
                        ]
                    }
                ]
            }
        },
        {
            "id": ")json" + kObjectActionWithRequirementsId + R"json(",
            "name": "Stub: Object Action with requirements",
            "supportedObjectTypeIds": [ ")json" + kObjectTypeId + R"json(" ],
            "requirements":
            {
                "capabilities": "needBestShotVideoFrame|needBestShotObjectMetadata|needFullTrack",
                "bestShotVideoFramePixelFormat": "yuv420"
            }
        }
    ],
    "typeLibrary":
    {
        "objectTypes":
        [
            {
                "id": ")json" + kObjectTypeId + R"json(",
                "name": "Stub: Object Type With Actions"
            }
        ]
    }
}
)json";
}

Result<IAction::Result> Engine::executeAction(
    const std::string& actionId,
    Uuid trackId,
    Uuid deviceId,
    int64_t timestampUs,
    Ptr<IObjectTrackInfo> objectTrackInfo,
    const std::map<std::string, std::string>& params)
{
    NX_PRINT << "Going to execute an Action. Action id: " << actionId
        << ", trackId: " << UuidHelper::toStdString(trackId)
        << ", deviceId: " << UuidHelper::toStdString(deviceId)
        << ", parameters: " << parametersToString(params);

    if (actionId == kObjectActionWithMessageResultId)
        return executeActionWithMessageResult(trackId, deviceId, timestampUs);

    if (actionId == kObjectActionWithUrlResultId)
        return executeActionWithUrlResult();

    if (actionId == kObjectActionWithParametersId)
        return executeActionWithParameters(trackId, deviceId, timestampUs, params);

    if (actionId == kObjectActionWithRequirementsId)
        return executeActionWithRequirements(trackId, deviceId, timestampUs, objectTrackInfo);

    return Result<IAction::Result>();
}

Result<IAction::Result> Engine::executeActionWithMessageResult(
    Uuid trackId,
    Uuid deviceId,
    int64_t timestampUs)
{
    IAction::Result result;

    const std::string messageToUser =
        "Message generated by the Plugin:\n"
        "    Track id: " + UuidHelper::toStdString(trackId) + ",\n"
        "    Device id: " + UuidHelper::toStdString(deviceId) + ",\n"
        "    Timestamp: " + nx::kit::utils::format("%lld us", timestampUs);

    result.messageToUser = makePtr<String>(messageToUser);

    NX_PRINT << "Executing an Action returning a message: "
        << nx::kit::utils::toString(messageToUser);

    return result;
}

Result<IAction::Result> Engine::executeActionWithUrlResult()
{
    IAction::Result result;

    static const std::string kUrl = "https://example.com";

    result.actionUrl = makePtr<String>(kUrl);

    NX_PRINT << "Executing an Action returning a URL: " << kUrl;

    return result;
}

Result<IAction::Result> Engine::executeActionWithParameters(
    Uuid trackId,
    Uuid deviceId,
    int64_t timestampUs,
    const std::map<std::string, std::string>& params)
{
    IAction::Result result;

    NX_PRINT << "Executing an Action with parameters.";

    std::string message = "Message generated by the Plugin:\n"
        "    Track id: " + UuidHelper::toStdString(trackId) + ",\n"
        "    Device id: " + UuidHelper::toStdString(deviceId) + ",\n"
        "    Timestamp: " + nx::kit::utils::format("%lld us", timestampUs);

    if (!params.empty())
    {
        message += ",\n"
            "    Parameters:\n";

        for (const auto& item: params)
        {
            const std::string& parameterName = item.first;
            const std::string& parameterValue = item.second;

            message += "        " + parameterName + ": [" + parameterValue + "],\n";
        }

        message = message.substr(0, message.size() - 2);
    }

    result.messageToUser = makePtr<String>(message);
    return result;
}

Result<IAction::Result> Engine::executeActionWithRequirements(
    Uuid trackId,
    Uuid deviceId,
    int64_t timestampUs,
    Ptr<IObjectTrackInfo> objectTrackInfo)
{
    IAction::Result result;

    std::string message = "Message generated by the Plugin:\n"
        "    Track id: " + UuidHelper::toStdString(trackId) + ",\n"
        "    Device id: " + UuidHelper::toStdString(deviceId) + ",\n"
        "    Timestamp: " + nx::kit::utils::format("%lld us", timestampUs) + ",\n"
        "    Object Track Info:";

    if (objectTrackInfo)
    {
        message += "\n        Best shot frame: "
            + uncompressedVideoFrameToString(objectTrackInfo->bestShotVideoFrame()) + "\n"
            + "        Best shot metadata: "
            + timestampedObjectMetadataToString(objectTrackInfo->bestShotObjectMetadata());
    }
    else
    {
        message += "null";
    }

    result.messageToUser = makePtr<String>(message);

    NX_PRINT << "Executing an Action with requirements, returning a message: "
        << nx::kit::utils::toString(message);
    return result;
}

} // namespace object_actions
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
