// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace roi {

static const std::string kDeviceAgentSettingsModel = /*suppress newline*/ 1 + R"json(
{
    "type": "Settings",
    "items":
    [
        {
            "type": "GroupBox",
            "caption": "Polygons",
            "items":
            [
                {
                    "type": "PolygonFigure",
                    "name": "excludedArea.figure",
                    "caption": "Excluded area",
                    "useLabelField": false,
                    "maxPoints": 8
                },
                {
                    "type": "Repeater",
                    "count": 5,
                    "template":
                    {
                        "type": "GroupBox",
                        "caption": "Polygon #",
                        "filledCheckItems": ["polygon#.figure"],
                        "items":
                        [
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
            "items":
            [
                {
                    "type": "Repeater",
                    "count": 5,
                    "template":
                    {
                        "type": "GroupBox",
                        "caption": "Box #",
                        "filledCheckItems": ["box#.figure"],
                        "items":
                        [
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
            "items":
            [
                {
                    "type": "Repeater",
                    "count": 5,
                    "template":
                    {
                        "type": "GroupBox",
                        "caption": "Line #",
                        "filledCheckItems": ["line#.figure"],
                        "items":
                        [
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
            "items":
            [
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
            "caption": "Polygon",
            "items":
            [
                {
                    "type": "PolygonFigure",
                    "name": "testPolygon",
                    "caption": "Polygon outside of a repeater",
                    "description": "The points of this polygon are considered as a plugin-side setting",
                    "minPoints": 3,
                    "maxPoints": 8
                }
            ]
        },
        {
            "type": "GroupBox",
            "caption": "Size Constraints",
            "items":
            [
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
)json";

} // namespace roi
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
