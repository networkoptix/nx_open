#include "engine.h"

#define NX_PRINT_PREFIX (this->logUtils.printPrefix)
#include <nx/kit/debug.h>

#include <nx/sdk/i_device_info.h>
#include <nx/sdk/analytics/helpers/plugin.h>
#include <nx/sdk/helpers/uuid_helper.h>
#include <nx/sdk/helpers/ref_countable.h>

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

Engine::Engine(nx::sdk::analytics::IPlugin* plugin):
    nx::sdk::analytics::Engine(plugin, NX_DEBUG_ENABLE_OUTPUT)
{
    initCapabilities();
}

Engine::~Engine()
{
    {
        std::unique_lock<std::mutex> lock(m_pluginEventGenerationLoopMutex);
        m_terminated = true;
        m_pluginEventGenerationLoopCondition.notify_all();
    }
    if (m_thread)
        m_thread->join();
}

void Engine::generatePluginEvents()
{
    while (!m_terminated)
    {
        using namespace std::chrono_literals;

        pushPluginEvent(
            IPluginEvent::Level::info,
            "Info message from Engine",
            "Info message description");

        pushPluginEvent(
            IPluginEvent::Level::warning,
            "Warning message from Engine",
            "Warning message description");

        pushPluginEvent(
            IPluginEvent::Level::error,
            "Error message from Engine",
            "Error message description");

        // Sleep until the next event pack needs to be generated, or the thread is ordered to
        // terminate (hence condition variable instead of sleep()). Return value (whether the
        // timeout has occurred) and spurious wake-ups are ignored.
        {
            std::unique_lock<std::mutex> lock(m_pluginEventGenerationLoopMutex);
            if (m_terminated)
                break;
            static const std::chrono::seconds kEventGenerationPeriod{7};
            m_pluginEventGenerationLoopCondition.wait_for(lock, kEventGenerationPeriod);
        }
    }
}

IDeviceAgent* Engine::obtainDeviceAgent(
    const IDeviceInfo* deviceInfo, Error* /*outError*/)
{
    return new DeviceAgent(this, deviceInfo);
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

std::string Engine::manifest() const
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
                "type": "TextField",
                "name": "testTextField",
                "caption": "Device Agent Text Field",
                "description": "A text field",
                "defaultValue": "a text"
            },
            {
                "type": "GroupBox",
                "caption": "Device Agent Group",
                "items": [
                    {
                        "type": "SpinBox",
                        "caption": "Device Agent SpinBox",
                        "name": "testSpinBox",
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

void Engine::settingsReceived()
{
    if (ini().throwPluginEventsFromEngine)
    {
        NX_PRINT << __func__ << "(): Starting plugin event generation thread";
        if (!m_thread)
            m_thread.reset(new std::thread([this]() { generatePluginEvents(); }));
    }
    else
    {
        NX_PRINT << __func__ << "()";
    }
}

static std::string timestampedObjectMetadataToString(const ITimestampedObjectMetadata* metadata)
{
    if (!metadata)
        return "null";

    return nx::kit::utils::format("timestamp: %lld, id: %s",
        metadata->timestampUs(), UuidHelper::toStdString(metadata->id()).c_str());
}

static std::string objectTrackToString(
    const IList<ITimestampedObjectMetadata>* track,
    Uuid expectedObjectId)
{
    using nx::kit::utils::format;

    if (!track)
        return "null";

    if (track->count() == 0)
        return "empty";

    std::string result = format("%d metadata items", track->count());

    Uuid objectIdFromTrack;
    for (int i = 0; i < track->count(); ++i)
    {
        const auto timestampedObjectMetadata = track->at(i);
        objectIdFromTrack = timestampedObjectMetadata->id();
        if (objectIdFromTrack != track->at(0)->id())
        {
            if (!result.empty())
                result += "; ";
            result += format("INTERNAL ERROR: Object id #%d %s does not equal object id #0 %s",
                i,
                UuidHelper::toStdString(objectIdFromTrack).c_str(),
                UuidHelper::toStdString(track->at(0)->id()).c_str());
            break;
        }
    }

    if (objectIdFromTrack != expectedObjectId)
    {
        if (!result.empty())
            result += "; ";
        result += format("INTERNAL ERROR: Object id in the track is %s, but in the action is %s",
            UuidHelper::toStdString(objectIdFromTrack).c_str(),
            UuidHelper::toStdString(expectedObjectId).c_str());
    }

    return result;
}

static std::string uncompressedVideoFrameToString(const IUncompressedVideoFrame* frame)
{
    if (!frame)
        return "null";

    return nx::kit::utils::format("%dx%d %s",
        frame->width(), frame->height(), pixelFormatToStdString(frame->pixelFormat()).c_str());
}

void Engine::executeAction(
    const std::string& actionId,
    Uuid objectId,
    Uuid /*deviceId*/,
    int64_t /*timestampUs*/,
    nx::sdk::Ptr<IObjectTrackInfo> objectTrackInfo,
    const std::map<std::string, std::string>& params,
    std::string* outActionUrl,
    std::string* outMessageToUser,
    Error* error)
{
    if (actionId == "nx.stub.addToList")
    {
        *outMessageToUser =
            std::string("Object id: ") + UuidHelper::toStdString(objectId) + "\n\n";

        if (!objectTrackInfo)
        {
            *outMessageToUser += "No object track info provided.\n\n";
        }
        else
        {
            *outMessageToUser += std::string("Object track info:\n")
                + "    Track: " + objectTrackToString(objectTrackInfo->track(), objectId) + "\n"
                + "    Best shot frame: "
                    + uncompressedVideoFrameToString(objectTrackInfo->bestShotVideoFrame()) + "\n"
                + "    Best shot metadata: "
                    + timestampedObjectMetadataToString(objectTrackInfo->bestShotObjectMetadata())
                + "\n\n";
        }

        if (!params.empty())
        {
            *outMessageToUser += std::string("Your param values are:\n");
            bool first = true;
            for (const auto& entry: params)
            {
                const auto& parameterName = entry.first;
                const auto& parameterValue = entry.second;

                if (!first)
                    *outMessageToUser += ",\n";
                else
                    first = false;
                *outMessageToUser += parameterName + ": [" + parameterValue + "]";
            }
        }
        else
        {
            *outMessageToUser += "No param values provided.";
        }

        NX_PRINT << __func__ << "(): Returning a message: "
            << nx::kit::utils::toString(*outMessageToUser);
    }
    else if (actionId == "nx.stub.addPerson")
    {
        *outActionUrl =
            "http://internal.server/addPerson?objectId=" + UuidHelper::toStdString(objectId);
        NX_PRINT << __func__ << "(): Returning URL: " << nx::kit::utils::toString(*outActionUrl);
    }
    else
    {
        NX_PRINT << __func__ << "(): ERROR: Unsupported actionId.";
        *error = Error::unknownError;
    }
}

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

namespace {

static const std::string kLibName = "stub_analytics_plugin";

static const std::string kPluginManifest = R"json(
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
                "type": "TextField",
                "name": "text",
                "caption": "Text Field",
                "description": "A text field",
                "defaultValue": "a text"
            },
            {
                "type": "GroupBox",
                "caption": "Group",
                "items": [
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
        kLibName,
        kPluginManifest,
        [](nx::sdk::analytics::IPlugin* plugin)
        {
            return new nx::vms_server_plugins::analytics::stub::Engine(plugin);
        });
}
