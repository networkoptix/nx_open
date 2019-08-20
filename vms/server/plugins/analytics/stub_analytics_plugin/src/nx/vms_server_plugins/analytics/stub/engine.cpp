// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "engine.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/string.h>
#include <nx/sdk/helpers/error.h>

#include "utils.h"
#include "device_agent.h"
#include "stub_analytics_plugin_ini.h"
#include "objects/vehicles.h"
#include "objects/human_face.h"

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

using namespace std::chrono;
using namespace std::literals::chrono_literals;

Engine::Engine(Plugin* plugin):
    nx::sdk::analytics::Engine(NX_DEBUG_ENABLE_OUTPUT),
    m_plugin(plugin)
{
    obtainPluginHomeDir();
    initCapabilities();

    m_pluginDiagnosticEventThread =
        std::make_unique<std::thread>([this]() { generatePluginDiagnosticEvents(); });
}

Engine::~Engine()
{
    {
        std::unique_lock<std::mutex> lock(m_pluginDiagnosticEventGenerationLoopMutex);
        m_terminated = true;
        m_pluginDiagnosticEventGenerationLoopCondition.notify_all();
    }
    if (m_pluginDiagnosticEventThread)
        m_pluginDiagnosticEventThread->join();
}

void Engine::generatePluginDiagnosticEvents()
{
    while (!m_terminated)
    {
        if (m_needToThrowPluginDiagnosticEvents)
        {
            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::info,
                "Info message from Engine",
                "Info message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::warning,
                "Warning message from Engine",
                "Warning message description");

            pushPluginDiagnosticEvent(
                IPluginDiagnosticEvent::Level::error,
                "Error message from Engine",
                "Error message description");
        }
        // Sleep until the next event pack needs to be generated, or the thread is ordered to
        // terminate (hence condition variable instead of sleep()). Return value (whether the
        // timeout has occurred) and spurious wake-ups are ignored.
        {
            std::unique_lock<std::mutex> lock(m_pluginDiagnosticEventGenerationLoopMutex);
            if (m_terminated)
                break;

            static const seconds kEventGenerationPeriod{7};
            m_pluginDiagnosticEventGenerationLoopCondition.wait_for(lock, kEventGenerationPeriod);
        }
    }
}

void Engine::doObtainDeviceAgent(Result<IDeviceAgent*>* outResult, const IDeviceInfo* deviceInfo)
{
    *outResult = new DeviceAgent(this, deviceInfo);
}

void Engine::obtainPluginHomeDir()
{
    const auto utilityProvider = m_plugin->utilityProvider();
    NX_KIT_ASSERT(utilityProvider);

    m_pluginHomeDir = utilityProvider->homeDir();

    if (m_pluginHomeDir.empty())
        NX_PRINT << "Plugin home dir: absent";
    else
        NX_PRINT << "Plugin home dir: " << nx::kit::utils::toString(m_pluginHomeDir);
}

void Engine::initCapabilities()
{
    if (ini().deviceDependent)
        m_capabilities += "|deviceDependent";

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

std::string Engine::manifestString() const
{
    return /*suppress newline*/1 + R"json(
{
    "eventTypes": [
        {
            "id": ")json" + kLineCrossingEventType + R"json(",
            "name": "Line crossing"
        },
        {
            "id": ")json" + kObjectInTheAreaEventType + R"json(",
            "name": "Object in the area",
            "flags": "stateDependent|regionDependent"
        },
        {
            "id": ")json" + kSuspiciousNoiseEventType + R"json(",
            "name": "Suspicious noise",
            "groupId": ")json" + kSoundRelatedEventGroup + R"json("
        }
    ],
    "objectTypes": [
        {
            "id": ")json" + kCarObjectType + R"json(",
            "name": "Car"
        },
        {
            "id": ")json" + kHumanFaceObjectType + R"json(",
            "name": "Human face"
        }
    ],
    "groups": [
        {
            "id": ")json" + kSoundRelatedEventGroup + R"json(",
            "name": "Sound related events"
        }
    ],
    "capabilities": ")json" + m_capabilities + R"json(",
    "objectActions": [
        {
            "id": "nx.stub.addToList",
            "name": "Add to list",
            "supportedObjectTypeIds": [
                ")json" + kCarObjectType + R"json("
            ],
            "requirements": {
                "capabilities": "needBestShotVideoFrame|needBestShotObjectMetadata|needTrack",
                "bestShotVideoFramePixelFormat": "yuv420"
            },
            "parametersModel": {
                "type": "Settings",
                "items": [
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
                        "items": [
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
                                "defaultValue": true,
                                "value": true
                            }
                        ]
                    }
                ]
            }
        },
        {
            "id": "nx.stub.addPerson",
            "name": "Add person (URL-based)",
            "supportedObjectTypeIds": [
                ")json" + kCarObjectType + R"json("
            ]
        }
    ],
    "deviceAgentSettingsModel": {
        "type": "Settings",
        "items": [
            {
                "type": "GroupBox",
                "caption": "Real Stub DeviceAgent settings",
                "items": [
                    {
                        "type": "GroupBox",
                        "caption": "Object generation settings",
                        "items": [
                            {
                                "type": "CheckBox",
                                "name": ")json" + kGenerateCarsSetting + R"json(",
                                "caption": "Generate cars",
                                "defaultValue": true,
                                "value": true
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json" + kGenerateTrucksSetting + R"json(",
                                "caption": "Generate trucks",
                                "defaultValue": true,
                                "value": true
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json" + kGeneratePedestriansSetting + R"json(",
                                "caption": "Generate pedestrians",
                                "defaultValue": true,
                                "value": true
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json" + kGenerateHumanFacesSetting + R"json(",
                                "caption": "Generate human faces",
                                "defaultValue": true,
                                "value": true
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json" + kGenerateBicyclesSetting + R"json(",
                                "caption": "Generate bicycles",
                                "defaultValue": true,
                                "value": true
                            },
                            {
                                "type": "SpinBox",
                                "name": ")json" + kNumberOfObjectsToGenerateSetting + R"json(",
                                "caption": "Number of objects to generate",
                                "defaultValue": 1,
                                "minValue": 1,
                                "maxValue": 100000
                            },
                            {
                                "type": "SpinBox",
                                "name": ")json" + kGenerateObjectsEveryNFramesSetting + R"json(",
                                "caption": "Generate objects every N frames",
                                "defaultValue": 1,
                                "minValue": 1,
                                "maxValue": 100000
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json" + kGeneratePreviewPacketSetting + R"json(",
                                "caption": "Generate preview packet",
                                "defaultValue": true,
                                "value": true
                            },
                            {
                                "type": "SpinBox",
                                "name": ")json" + kGeneratePreviewAfterNFramesSetting + R"json(",
                                "caption": "Generate preview after N frames",
                                "defaultValue": 30,
                                "minValue": 1,
                                "maxValue": 100000
                            }
                        ]
                    },
                    {
                        "type": "CheckBox",
                        "name": ")json" + kGenerateEventsSetting + R"json(",
                        "caption": "Generate events",
                        "defaultValue": true,
                        "value": true
                    },
                    {
                        "type": "CheckBox",
                        "name": ")json"
                        + kThrowPluginDiagnosticEventsFromDeviceAgentSetting + R"json(",
                        "caption": "Throw plugin events from the DeviceAgent",
                        "defaultValue": false,
                        "value": false
                    },
                    {
                        "type": "CheckBox",
                        "name": ")json" + kLeakFrames + R"json(",
                        "caption": "Force a memory leak when proccessing a video frame",
                        "defaultValue": false,
                        "value": false
                    },
                    {
                        "type": "SpinBox",
                        "name": ")json" + kAdditionalFrameProcessingDelayMs + R"json(",
                        "caption": "Additional frame processing delay, ms",
                        "defaultValue": 0,
                        "value": 0
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "Example Stub DeviceAgent settings",
                "items": [
                    {
                        "type": "TextField",
                        "name": "testTextField",
                        "caption": "Device Agent Text Field",
                        "description": "A text field",
                        "defaultValue": "a text"
                    },
                    {
                        "type": "SpinBox",
                        "caption": "Device Agent SpinBox (plugin side)",
                        "name": "pluginSideTestSpinBox",
                        "defaultValue": 42,
                        "minValue": 0,
                        "maxValue": 100
                    },
                    {
                        "type": "DoubleSpinBox",
                        "caption": "Device Agent DoubleSpinBox",
                        "name": "testDoubleSpinBox",
                        "defaultValue": 3.1415,
                        "minValue": 0.0,
                        "maxValue": 100.0
                    },
                    {
                        "type": "ComboBox",
                        "name": "testComboBox",
                        "caption": "Device Agent ComboBox",
                        "defaultValue": "value2",
                        "range": ["value1", "value2", "value3"]
                    },
                    {
                        "type": "CheckBox",
                        "caption": "Device Agent CheckBox",
                        "name": "testCheckBox",
                        "defaultValue": true,
                        "value": true
                    }
                ]
            }
        ]
    }
}
)json";
}

Result<const IStringMap*> Engine::settingsReceived()
{
    m_needToThrowPluginDiagnosticEvents = toBool(
        settingValue(kThrowPluginDiagnosticEventsFromEngineSetting));

    if (m_needToThrowPluginDiagnosticEvents && !m_pluginDiagnosticEventThread)
    {
        NX_PRINT << __func__ << "(): Starting plugin event generation";
        m_needToThrowPluginDiagnosticEvents = true;
    }
    else if (!m_needToThrowPluginDiagnosticEvents && m_pluginDiagnosticEventThread)
    {
        NX_PRINT << __func__ << "(): Stopping plugin event generation";
        m_needToThrowPluginDiagnosticEvents = false;
        m_pluginDiagnosticEventGenerationLoopCondition.notify_all();
    }

    return nullptr;
}

static std::string timestampedObjectMetadataToString(
    Ptr<const ITimestampedObjectMetadata> metadata)
{
    if (!metadata)
        return "null";

    return nx::kit::utils::format("timestamp: %lld, id: %s",
        metadata->timestampUs(), UuidHelper::toStdString(metadata->trackId()).c_str());
}

static std::string objectTrackToString(
    Ptr<const IList<ITimestampedObjectMetadata>> track,
    Uuid expectedTrackId)
{
    using nx::kit::utils::format;

    if (!track)
        return "null";

    if (track->count() == 0)
        return "empty";

    const auto firstMetadataOfTrack = track->at(0);
    if (!firstMetadataOfTrack)
        return "INTERNAL ERROR: Invalid Track metadata list";

    std::string result = format(
        "%d metadata items, Track start time, us: %lld",
        track->count(),
        firstMetadataOfTrack->timestampUs());

    Uuid trackld;
    for (int i = 0; i < track->count(); ++i)
    {
        const auto timestampedObjectMetadata = track->at(i);
        trackld = timestampedObjectMetadata->trackId();
        if (trackld != track->at(0)->trackId())
        {
            if (!result.empty())
                result += "; ";
            result += format("INTERNAL ERROR: Track id #%d %s does not equal Track id #0 %s",
                i,
                UuidHelper::toStdString(trackld).c_str(),
                UuidHelper::toStdString(track->at(0)->trackId()).c_str());
            break;
        }
    }

    if (trackld != expectedTrackId)
    {
        if (!result.empty())
            result += "; ";
        result += format("INTERNAL ERROR: Track id in the Track is %s, but in the Action is %s",
            UuidHelper::toStdString(trackld).c_str(),
            UuidHelper::toStdString(expectedTrackId).c_str());
    }

    return result;
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
                + "    Track: " + objectTrackToString(objectTrackInfo->track(), trackId) + "\n"
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
        actionUrl =
            "http://internal.server/addPerson?trackId=" + UuidHelper::toStdString(trackId);
        NX_PRINT << __func__ << "(): Returning URL: " << nx::kit::utils::toString(actionUrl);
    }
    else
    {
        NX_PRINT << __func__ << "(): ERROR: Unsupported actionId.";
        return error(ErrorCode::invalidParams, "Unsupported actionId");
    }

    return IAction::Result{makePtr<String>(actionUrl), makePtr<String>(messageToUser)};
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

namespace {

using namespace nx::vms_server_plugins::analytics::stub;

static const std::string kPluginManifest = /*suppress newline*/ 1 + R"json(
{
    "id": "nx.stub",
    "name": "Stub analytics plugin",
    "description": "A plugin for testing and debugging purposes",
    "version": "1.0.0",
    "vendor": "Plugin vendor",
    "engineSettingsModel": {
        "type": "Settings",
        "items": [
            {
                "type": "GroupBox",
                "caption": "Real Stub Engine settings",
                "items": [
                    {
                        "type": "CheckBox",
                        "name": ")json" + kThrowPluginDiagnosticEventsFromEngineSetting + R"json(",
                        "caption": "Throw plugin events from the Engine",
                        "defaultValue": false,
                        "value": false
                    }
                ]
            },
            {
                "type": "GroupBox",
                "caption": "Example Stub Engine settings",
                "items": [
                    {
                        "type": "TextField",
                        "name": "text",
                        "caption": "Text Field",
                        "description": "A text field",
                        "defaultValue": "a text"
                    },
                    {
                        "type": "SpinBox",
                        "name": "testSpinBox",
                        "caption": "Spin Box",
                        "defaultValue": 42,
                        "minValue": 0,
                        "maxValue": 100
                    },
                    {
                        "type": "DoubleSpinBox",
                        "name": "testDoubleSpinBox",
                        "caption": "Double Spin Box",
                        "defaultValue": 3.1415,
                        "minValue": 0.0,
                        "maxValue": 100.0
                    },
                    {
                        "type": "ComboBox",
                        "name": "testComboBox",
                        "caption": "Combo Box",
                        "defaultValue": "value2",
                        "range": ["value1", "value2", "value3"]
                    },
                    {
                        "type": "CheckBox",
                        "name": "testCheckBox",
                        "caption": "Check Box",
                        "defaultValue": true,
                        "value": true
                    }
                ]
            }
        ]
    }
})json";

} // namespace

extern "C" NX_PLUGIN_API nx::sdk::IPlugin* createNxPlugin()
{
    return new nx::sdk::analytics::Plugin(
        kPluginManifest,
        [](nx::sdk::analytics::Plugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::stub::Engine(plugin);
        });
}
