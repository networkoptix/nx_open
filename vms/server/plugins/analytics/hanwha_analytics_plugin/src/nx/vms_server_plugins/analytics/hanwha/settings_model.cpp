#include "settings_model.h"

#include <chrono>
#include <sstream>
#include <iostream>
#include <charconv>

#include <QString>

#define NX_PRINT_PREFIX "[hanwha::DeviceAgent] "
#include <nx/kit/debug.h>

namespace nx::vms_server_plugins::analytics::hanwha {

using namespace nx::sdk;
using namespace nx::sdk::analytics;

//    "deviceAgentSettingsModel":
const QString kModelPattern = R"json(
    {
        "type": "Settings",
%1,
%2
    }
)json";

const QString kDefaultSectionModelPattern = R"json(
        "items":
        [
%1
        ]
)json";

const QString kSectionsModelPattern = R"json(
        "sections":
        [
%1
        ]
)json";

const QString kDefaultSectionModelStuffing = R"json(
            {
                "type": "GroupBox",
                "caption": "Common settings: Motion and IVA",
                "items":
                [
                    {
                        "type": "ComboBox",
                        "name": "MotionDetection.DetectionType",
                        "caption": "Motion detection type",
                        "range": ["Off", "MotionDetection", "IntelligentVideo", "MDAndIV"],
                        "defaultValue": "MotionDetection"
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Object size restraints for Motion detection",
                        "items":
                        [
                            {
                                "type": "BoxFigure",
                                "name": "MotionDetection.MinObjectSize.Points",
                                "caption": "Motion - Minimum object size"
                            },
                            {
                                "type": "BoxFigure",
                                "name": "MotionDetection.MaxObjectSize.Points",
                                "caption": "Motion - Maximum object size"
                            }
                        ]
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Object size restraints for AI (Object detection and IVA)",
                        "items":
                        [
                            {
                                "type": "BoxFigure",
                                "name": "IVA.MinObjectSize.Points",
                                "caption": "AI - Minimum object size"
                            },
                            {
                                "type": "BoxFigure",
                                "name": "IVA.MaxObjectSize.Points",
                                "caption": "AI - Maximum object size"
                            }
                        ]
                    }
                ]
            }
)json";

const QString kShockDetectionModel = R"json(
            {
                "type": "Section",
                "name": "1 Shock detection",
                "items":
                [
                    {
                        "type": "CheckBox",
                        "name": "ShockDetection.Enable",
                        "caption": "Enable",
                        "defaultValue": false
                    },
                    {
                        "type": "SpinBox",
                        "name": "ShockDetection.ThresholdLevel",
                        "caption": "Level of detection",
                        "defaultValue": 50,
                        "minValue": 1,
                        "maxValue": 100
                    },
                    {
                        "type": "SpinBox",
                        "name": "ShockDetection.SensitivityLevel",
                        "caption": "Sensitivity",
                        "defaultValue": 80,
                        "minValue": 1,
                        "maxValue": 100
                    }
                ]
            }
)json";

const QString kTamperingDetectionModel = R"json(
            {
                "type": "Section",
                "name": "3 Tampering detection",
                "items":
                [
                    {
                        "type": "CheckBox",
                        "name": "TamperingDetection.Enable",
                        "caption": "Enable",
                        "defaultValue": false
                    },
                    {
                        "type": "SpinBox",
                        "name": "TamperingDetection.ThresholdLevel",
                        "caption": "Level of detection",
                        "defaultValue": 50,
                        "minValue": 1,
                        "maxValue": 100
                    },
                    {
                        "type": "SpinBox",
                        "name": "TamperingDetection.SensitivityLevel",
                        "caption": "Sensitivity",
                        "defaultValue": 80,
                        "minValue": 1,
                        "maxValue": 100
                    },
                    {
                        "type": "SpinBox",
                        "name": "TamperingDetection.MinimumDuration",
                        "caption": "Minimum duration (s)",
                        "defaultValue": 1,
                        "minValue": 1,
                        "maxValue": 5
                    },
                    {
                        "type": "CheckBox",
                        "name": "TamperingDetection.ExceptDarkImages",
                        "caption": "Except dark images",
                        "defaultValue": false,
                        "value": false
                    }
                ]
            }
)json";

const QString kDefocusDetectionModel = R"json(
            {
                "type": "Section",
                "name": "4 Defocus detection",
                "items":
                [
                    {
                        "type": "CheckBox",
                        "name": "DefocusDetection.Enable",
                        "caption": "Enable",
                        "defaultValue": false
                    },
                    {
                        "type": "SpinBox",
                        "name": "DefocusDetection.ThresholdLevel",
                        "caption": "Level of detection",
                        "defaultValue": 20,
                        "minValue": 1,
                        "maxValue": 100
                    },
                    {
                        "type": "SpinBox",
                        "name": "DefocusDetection.SensitivityLevel",
                        "caption": "Sensitivity",
                        "defaultValue": 80,
                        "minValue": 1,
                        "maxValue": 100
                    },
                    {
                        "type": "SpinBox",
                        "name": "DefocusDetection.MinimumDuration",
                        "caption": "Minimum Duration (s)",
                        "defaultValue": 0,
                        "minValue": 1,
                        "maxValue": 5
                    }
                ]
            }
)json";

const QString testModel = R"json(
    {
        "type": "Settings",
        "items":
        [
            {
                "type": "GroupBox",
                "caption": "Common settings: Motion and IVA",
                "items":
                [
                    {
                        "type": "ComboBox",
                        "name": "MotionDetection.DetectionType",
                        "caption": "Motion detection type",
                        "range": ["Off", "MotionDetection", "IntelligentVideo", "MDAndIV"],
                        "defaultValue": "MotionDetection"
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Object size restraints for Motion detection",
                        "items":
                        [
                            {
                                "type": "BoxFigure",
                                "name": "MotionDetection.MinObjectSize.Points",
                                "caption": "Motion - Minimum object size"
                            },
                            {
                                "type": "BoxFigure",
                                "name": "MotionDetection.MaxObjectSize.Points",
                                "caption": "Motion - Maximum object size"
                            }
                        ]
                    },
                    {
                        "type": "GroupBox",
                        "caption": "Object size restraints for AI (Object detection and IVA)",
                        "items":
                        [
                            {
                                "type": "BoxFigure",
                                "name": "IVA.MinObjectSize.Points",
                                "caption": "AI - Minimum object size"
                            },
                            {
                                "type": "BoxFigure",
                                "name": "IVA.MaxObjectSize.Points",
                                "caption": "AI - Maximum object size"
                            }
                        ]
                    }
                ]
            }
        ],
        "sections":
        [
            {
                "type": "Section",
                "name": "1 Shock detection",
                "items":
                [
                    {
                        "type": "CheckBox",
                        "name": "ShockDetection.Enable",
                        "caption": "Enable",
                        "defaultValue": false
                    },
                    {
                        "type": "SpinBox",
                        "name": "ShockDetection.ThresholdLevel",
                        "caption": "Level of detection",
                        "defaultValue": 50,
                        "minValue": 1,
                        "maxValue": 100
                    },
                    {
                        "type": "SpinBox",
                        "name": "ShockDetection.SensitivityLevel",
                        "caption": "Sensitivity",
                        "defaultValue": 80,
                        "minValue": 1,
                        "maxValue": 100
                    }
                ]
            },
            {
                "type": "Section",
                "name": "2.1 Motion detection. Include areas",
                "items":
                [
                    {
                        "type": "GroupBox",
                        "caption": "Include area 1",
                        "items":
                        [
                            {
                              "type": "PolygonFigure",
                              "minPoints": 4,
                              "maxPoints": 8,
                              "name": "MotionDetection.IncludeArea1.Points",
                              "caption": "Include area"
                            },
                            {
                                "type": "SpinBox",
                                "name": "MotionDetection.IncludeArea1.ThresholdLevel",
                                "caption": "Level of detection",
                                "defaultValue": 50,
                                "minValue": 1,
                                "maxValue": 100
                            },
                            {
                                "type": "SpinBox",
                                "name": "MotionDetection.IncludeArea1.SensitivityLevel",
                                "caption": "Sensitivity",
                                "defaultValue": 80,
                                "minValue": 1,
                                "maxValue": 100
                            },
                            {
                                "type": "SpinBox",
                                "name": "MotionDetection.IncludeArea1.MinimumDuration",
                                "caption": "Minimum duration (s)",
                                "defaultValue": 0,
                                "minValue": 0,
                                "maxValue": 5
                            }
                        ]
                    }
                ]
            }
        ]
    }
)json";

QString deviceAgentSettingsModel()
{
    const QString defaultSectionModel = kDefaultSectionModelPattern.arg(
        kDefaultSectionModelStuffing);

    const QString sectionsModelStuffing = kShockDetectionModel +
        "," + kTamperingDetectionModel +
        "," + kDefocusDetectionModel;

    const QString sectionsModel = kSectionsModelPattern.arg(sectionsModelStuffing);

    const QString model = kModelPattern.arg(defaultSectionModel, sectionsModel);

    return testModel;//    return model;
}

} // namespace nx::vms_server_plugins::analytics::hanwha
