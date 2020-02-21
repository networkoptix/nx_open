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
#include "objects/stone.h"

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

    m_streamTypeFilter = "motion|compressedVideo";

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
            m_streamTypeFilter = "motion|uncompressedVideo";
        }
    }

    if (ini().needMetadata)
        m_streamTypeFilter += "|metadata";

    // Delete first '|', if any.
    if (!m_capabilities.empty() && m_capabilities.at(0) == '|')
        m_capabilities.erase(0, 1);
}

std::string Engine::manifestString() const
{
    return /*suppress newline*/ 1 + R"json(
{
    "eventTypes": [
        {
            "id": ")json"
        + kLineCrossingEventType + R"json(",
            "name": "Line crossing"
        },
        {
            "id": ")json"
        + kObjectInTheAreaEventType + R"json(",
            "name": "Object in the area",
            "flags": "stateDependent|regionDependent"
        },
        {
            "id": ")json"
        + kSuspiciousNoiseEventType + R"json(",
            "name": "Suspicious noise",
            "groupId": ")json"
        + kSoundRelatedEventGroup + R"json("
        }
    ],
    "objectTypes": [
        {
            "id": ")json"
        + kCarObjectType + R"json(",
            "name": "Car"
        },
        {
            "id": ")json"
        + kHumanFaceObjectType + R"json(",
            "name": "Human face"
        },
        {
            "id": ")json"
        + kMotionVisualizationObjectType + R"json(",
            "name": "Motion visualization object"
        },
        {
            "id": ")json"
        + kStoneObjectType + R"json(",
            "name": "Stone"
        }
    ],
    "groups": [
        {
            "id": ")json"
        + kSoundRelatedEventGroup + R"json(",
            "name": "Sound related events"
        }
    ],
    "capabilities": ")json"
        + m_capabilities + R"json(",
    "streamTypeFilter": ")json"
        + m_streamTypeFilter + R"json(",
    "preferredStream": ")json"
        + ini().preferredStream + R"json(",
    "objectActions": [
        {
            "id": "nx.stub.addToList",
            "name": "Add to list",
            "supportedObjectTypeIds": [
                ")json"
        + kCarObjectType + R"json("
            ],
            "requirements": {
                "capabilities": "needBestShotVideoFrame|needBestShotObjectMetadata|needFullTrack",
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
            "supportedObjectTypeIds": [
                ")json"
        + kCarObjectType + R"json("
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
                                "name": ")json"
        + kGenerateCarsSetting + R"json(",
                                "caption": "Generate cars",
                                "defaultValue": true
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json"
        + kGenerateTrucksSetting + R"json(",
                                "caption": "Generate trucks",
                                "defaultValue": true
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json"
        + kGeneratePedestriansSetting + R"json(",
                                "caption": "Generate pedestrians",
                                "defaultValue": true
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json"
        + kGenerateHumanFacesSetting + R"json(",
                                "caption": "Generate human faces",
                                "defaultValue": true
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json"
        + kGenerateBicyclesSetting + R"json(",
                                "caption": "Generate bicycles",
                                "defaultValue": true
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json"
        + kGenerateStonesSetting + R"json(",
                                "caption": "Generate stones",
                                "defaultValue": false
                            },
                            {
                                "type": "SpinBox",
                                "name": ")json"
        + kBlinkingObjectPeriodMsSetting + R"json(",
                                "caption": "Generate 1-frame BlinkingObject every N ms (if not 0)",
                                "defaultValue": 0,
                                "minValue": 0,
                                "maxValue": 100000
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json"
        + kBlinkingObjectInDedicatedPacketSetting + R"json(",
                                "caption": "Put BlinkingObject into a dedicated MetadataPacket",
                                "defaultValue": false
                            },
                            {
                                "type": "SpinBox",
                                "name": ")json"
        + kNumberOfObjectsToGenerateSetting + R"json(",
                                "caption": "Number of objects to generate",
                                "defaultValue": 1,
                                "minValue": 1,
                                "maxValue": 100000
                            },
                            {
                                "type": "SpinBox",
                                "name": ")json"
        + kGenerateObjectsEveryNFramesSetting + R"json(",
                                "caption": "Generate objects every N frames",
                                "defaultValue": 1,
                                "minValue": 1,
                                "maxValue": 100000
                            },
                            {
                                "type": "CheckBox",
                                "name": ")json"
        + kGeneratePreviewPacketSetting + R"json(",
                                "caption": "Generate preview packet",
                                "defaultValue": true
                            },
                            {
                                "type": "SpinBox",
                                "name": ")json"
        + kGeneratePreviewAfterNFramesSetting + R"json(",
                                "caption": "Generate preview after N frames",
                                "defaultValue": 30,
                                "minValue": 1,
                                "maxValue": 100000
                            },
                            {
                                "type": "SpinBox",
                                "name": ")json"
        + kOverallMetadataDelayMsSetting + R"json(",
                                "caption": "Overall metadata delay, ms",
                                "defaultValue": 0,
                                "minValue": 0,
                                "maxValue": 1000000000
                            }
                        ]
                    },
                    {
                        "type": "CheckBox",
                        "name": ")json"
        + kGenerateEventsSetting + R"json(",
                        "caption": "Generate events",
                        "defaultValue": true
                    },
                    {
                        "type": "CheckBox",
                        "name": ")json"
        + kThrowPluginDiagnosticEventsFromDeviceAgentSetting + R"json(",
                        "caption": "Produce Plugin Diagnostic Events from the DeviceAgent",
                        "defaultValue": false
                    },
                    {
                        "type": "CheckBox",
                        "name": ")json"
        + kLeakFramesSetting + R"json(",
                        "caption": "Force a memory leak when processing a video frame",
                        "defaultValue": false
                    },
                    {
                        "type": "SpinBox",
                        "name": ")json"
        + kAdditionalFrameProcessingDelayMsSetting + R"json(",
                        "caption": "Additional frame processing delay, ms",
                        "defaultValue": 0,
                        "minValue": 0,
                        "maxValue": 1000000000
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
                        "type": "RadioButtonGroup",
                        "name": "testRadioButtonGroup",
                        "caption": "RadioButton Group",
                        "description": "Choose one option",
                        "defaultValue": "Cs_enodatum",
                        "range": [
                            "K_gowerianus",
                            "K_galilaeii",
                            "S_calloviense",
                            "S_micans",
                            "Cs_enodatum",
                            "K_medea",
                            "K_jason",
                            "K_obductum",
                            "K_posterior"
                        ],
                        "itemCaptions": {
                            "K_gowerianus": "Kepplerites gowerianus",
                            "K_galilaeii": "Kepplerites galilaeii",
                            "S_calloviense": "Sigaloceras calloviense",
                            "S_micans": "Sigaloceras micans",
                            "Cs_enodatum": "Catasigaloceras enodatum",
                            "K_medea": "Kosmoceras medea",
                            "K_jason": "Kosmoceras jason",
                            "K_obductum": "Kosmoceras obductum",
                            "K_posterior": "Kosmoceras posterior"
                        }
                    },
                    {
                        "type": "CheckBoxGroup",
                        "name": "testCheckBoxGroup",
                        "caption": "CheckBox Group",
                        "description": "Choose one or several options",
                        "defaultValue": ["Coleoidea", "Nautiloidea"],
                        "range": [
                            "Coleoidea",
                            "Ammonoidea",
                            "Nautiloidea",
                            "Orthoceratoidea"
                        ],
                        "itemCaptions": {
                            "Coleoidea": "Coleoidea (Bather, 1888)",
                            "Ammonoidea": "Ammonoidea (Zittel, 1884)",
                            "Nautiloidea": "Nautiloidea (Agassiz, 1847)",
                            "Orthoceratoidea": "Orthoceratoidea (M'Coy 1844)"
                        }
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
                        "range": ["value1", "value2", "value3"],
                        "itemCaptions": {
                            "value1": "Device Agent Value #1",
                            "value2": "Device Agent Value #2",
                            "value3": "Device Agent Value #3"
                        }
                    },
                    {
                        "type": "Separator"
                    },
                    {
                        "type": "CheckBox",
                        "caption": "Device Agent CheckBox",
                        "name": "testCheckBox",
                        "defaultValue": true
                    }
                ]
            }
        ],)json" + R"json(
        "sections": [
            {
                "type": "Section",
                "caption": "Example",
                "items": [
                    {
                        "type": "GroupBox",
                        "caption": "Example Stub DeviceAgent settings",
                        "items": [
                            {
                                "type": "TextField",
                                "name": "testTextFieldWithValidation",
                                "caption": "Hexadecimal number text field",
                                "defaultValue": "12ab34cd",
                                "validationRegex": "^[a-f0-9]+$",
                                "validationRegexFlags": "i",
                                "validationErrorMessage": "Text must contain only digits and characters a-f, e.g. 12ab34cd."
                            },
                            {
                                "type": "SpinBox",
                                "caption": "Device Agent SpinBox (plugin side)",
                                "name": "pluginSideTestSpinBox2",
                                "defaultValue": 42,
                                "minValue": 0,
                                "maxValue": 100
                            },
                            {
                                "type": "DoubleSpinBox",
                                "caption": "Device Agent DoubleSpinBox",
                                "name": "testDoubleSpinBox2",
                                "defaultValue": 3.1415,
                                "minValue": 0.0,
                                "maxValue": 100.0
                            },
                            {
                                "type": "ComboBox",
                                "name": "testComboBox2",
                                "caption": "Device Agent ComboBox",
                                "defaultValue": "value2",
                                "range": ["value1", "value2", "value3"]
                            },
                            {
                                "type": "CheckBox",
                                "caption": "Device Agent CheckBox",
                                "name": "testCheckBox2",
                                "defaultValue": true
                            }
                        ]
                    }
                ],
                "sections": [
                    {
                        "type": "Section",
                        "caption": "Nested section",
                        "items": [
                            {
                                "type": "GroupBox",
                                "caption": "Nested Section Example",
                                "items": [
                                    {
                                        "type": "SpinBox",
                                        "caption": "SpinBox Parameter",
                                        "name": "testSpinBox3",
                                        "defaultValue": 42,
                                        "minValue": 0,
                                        "maxValue": 100
                                    },
                                    {
                                        "type": "DoubleSpinBox",
                                        "caption": "DoubleSpinBox Parameter",
                                        "name": "testDoubleSpinBox3",
                                        "defaultValue": 3.1415,
                                        "minValue": 0.0,
                                        "maxValue": 100.0
                                    },
                                    {
                                        "type": "ComboBox",
                                        "name": "testComboBox3",
                                        "caption": "ComboBox Parameter",
                                        "defaultValue": "value2",
                                        "range": ["value1", "value2", "value3"]
                                    },
                                    {
                                        "type": "CheckBox",
                                        "caption": "CheckBox Parameter",
                                        "name": "testCheckBox3",
                                        "defaultValue": true
                                    }
                                ]
                            }
                        ]
                    }
                ]
            },
            {
                "type": "Section",
                "caption": "ROI",
                "items": [
                    {
                        "type": "GroupBox",
                        "caption": "Polygons",
                        "items": [
                            {
                                "type": "Repeater",
                                "count": 5,
                                "template": {
                                    "type": "GroupBox",
                                    "caption": "Polygon #",
                                    "filledCheckItems": ["polygon#.figure"],
                                    "items": [
                                        {
                                            "type": "PolygonFigure",
                                            "name": "polygon#.figure",
                                            "minPoints": 4,
                                            "maxPoints": 8
                                        },
                                        {
                                            "type": "SpinBox",
                                            "name": "polygon#.threshold",
                                            "caption": "Level of detection",
                                            "defaultValue": 50,
                                            "minValue": 1,
                                            "maxValue": 100
                                        },
                                        {
                                            "type": "SpinBox",
                                            "name": "polygon#.sensitivity",
                                            "caption": "Sensitivity",
                                            "defaultValue": 80,
                                            "minValue": 1,
                                            "maxValue": 100
                                        },
                                        {
                                            "type": "SpinBox",
                                            "name": "polygon#.minimumDuration",
                                            "caption": "Minimum duration (s)",
                                            "defaultValue": 0,
                                            "minValue": 0,
                                            "maxValue": 5
                                        }
                                    ]
                                }
                            }
                        ]
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Boxes",
                        "items": [
                            {
                                "type": "Repeater",
                                "count": 5,
                                "template": {
                                    "type": "GroupBox",
                                    "caption": "Box #",
                                    "filledCheckItems": ["box#.figure"],
                                    "items": [
                                        {
                                            "type": "BoxFigure",
                                            "name": "box#.figure"
                                        },
                                        {
                                            "type": "SpinBox",
                                            "name": "box#.threshold",
                                            "caption": "Level of detection",
                                            "defaultValue": 50,
                                            "minValue": 1,
                                            "maxValue": 100
                                        },
                                        {
                                            "type": "SpinBox",
                                            "name": "box#.sensitivity",
                                            "caption": "Sensitivity",
                                            "defaultValue": 80,
                                            "minValue": 1,
                                            "maxValue": 100
                                        },
                                        {
                                            "type": "SpinBox",
                                            "name": "box#.minimumDuration",
                                            "caption": "Minimum duration (s)",
                                            "defaultValue": 0,
                                            "minValue": 0,
                                            "maxValue": 5
                                        }
                                    ]
                                }
                            }
                        ]
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Lines",
                        "items": [
                            {
                                "type": "Repeater",
                                "count": 5,
                                "template": {
                                    "type": "GroupBox",
                                    "caption": "Line #",
                                    "filledCheckItems": ["line#.figure"],
                                    "items": [
                                        {
                                            "type": "LineFigure",
                                            "name": "line#.figure"
                                        },
                                        {
                                            "type": "CheckBox",
                                            "name": "line#.person",
                                            "caption": "Person",
                                            "defaultValue": false
                                        },
                                        {
                                            "type": "CheckBox",
                                            "name": "line#.vehicle",
                                            "caption": "Vehicle",
                                            "defaultValue": false
                                        },
                                        {
                                            "type": "CheckBox",
                                            "name": "line#.crossing",
                                            "caption": "Crossing",
                                            "defaultValue": false
                                        }
                                    ]
                                }
                            }
                        ]
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Polyline",
                        "items": [
                            {
                                "type": "LineFigure",
                                "name": "testPolyLine",
                                "caption": "Polyline",
                                "maxPoints": 8
                            }
                        ]
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Size Constraints",
                        "items": [
                            {
                                "type": "ObjectSizeConstraints",
                                "name": "testSizeConstraints",
                                "caption": "Object size constraints",
                                "description": "Size range an object should fit into to be detected",
                                "defaultValue": {"minimum": [0.1, 0.4], "maximum": [0.2, 0.8]}
                            }
                        ]
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
        NX_PRINT << __func__ << "(): Starting Plugin Diagnostic Event generation";
        m_needToThrowPluginDiagnosticEvents = true;
    }
    else if (!m_needToThrowPluginDiagnosticEvents && m_pluginDiagnosticEventThread)
    {
        NX_PRINT << __func__ << "(): Stopping Plugin Diagnostic Event generation";
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

} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
