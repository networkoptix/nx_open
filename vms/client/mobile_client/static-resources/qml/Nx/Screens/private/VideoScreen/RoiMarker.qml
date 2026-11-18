// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core.Items

// Displays a region of interest: a frame between two points, or a crosshair if it's a single
// point. While the region is being drawn, its corners are marked with dots.
Item
{
    id: item

    property point startPoint
    property point endPoint
    property bool drawing: false
    property bool singlePoint: true

    property color roiColor
    property color shadowColor

    x: d.topLeftPoint.x
    y: d.topLeftPoint.y

    width: d.bottomRightPoint.x - d.topLeftPoint.x + 1
    height: d.bottomRightPoint.y - d.topLeftPoint.y + 1

    // A filled circle with a thin shadow outline.
    component ShadedCircle: Circle
    {
        id: shadedCircle

        property color fillColor
        property color shadowColor

        radius: dot.radius + border.width
        border.width: 1
        borderColor: shadowColor

        Circle
        {
            id: dot

            anchors.centerIn: parent
            radius: 2.5
            circleColor: shadedCircle.fillColor
        }
    }

    // Never expanded. Fades in by its own opacity animation.
    RoiCrosshair
    {
        id: singlePointMarker

        color: item.roiColor
        shadowColor: item.shadowColor
        centerPoint: d.endMarkerPoint

        // Hidden at once when drawing starts, otherwise it would fade out at the new point.
        visible: !item.drawing && opacity > 0
        opacity: item.singlePoint ? 1 : 0
    }

    Rectangle
    {
        id: frame

        visible: item.drawing || !item.singlePoint
        anchors.fill: parent
        border.width: d.shadowRadius * 2 + boundingLines.border.width
        border.color: item.shadowColor
        color: "transparent"

        Rectangle
        {
            id: boundingLines

            x: d.shadowRadius
            y: d.shadowRadius
            width: parent.width - d.shadowRadius * 2
            height: parent.height - d.shadowRadius * 2
            color: "transparent"

            border.width: 1
            border.color: item.roiColor
        }

        ShadedCircle
        {
            visible: item.drawing
            centerPoint: d.startMarkerPoint
            fillColor: item.roiColor
            shadowColor: item.shadowColor
        }

        ShadedCircle
        {
            visible: item.drawing
            centerPoint: d.endMarkerPoint
            fillColor: item.roiColor
            shadowColor: item.shadowColor
        }
    }

    QtObject
    {
        id: d

        readonly property int shadowRadius: 1
        readonly property real subpixelOffset: 0.5
        readonly property real markerOffset: shadowRadius + subpixelOffset
        readonly property real markerRight: item.width - shadowRadius - subpixelOffset
        readonly property real markerBottom: item.height - shadowRadius - subpixelOffset

        readonly property point topLeftPoint: Qt.point(
            Math.min(item.startPoint.x, item.endPoint.x) - shadowRadius,
            Math.min(item.startPoint.y, item.endPoint.y) - shadowRadius)

        readonly property point bottomRightPoint: Qt.point(
            Math.max(item.startPoint.x, item.endPoint.x) + shadowRadius,
            Math.max(item.startPoint.y, item.endPoint.y) + shadowRadius)

        readonly property point startMarkerPoint: Qt.point(
            item.startPoint.x < item.endPoint.x ? markerOffset : markerRight,
            item.startPoint.y < item.endPoint.y ? markerOffset : markerBottom)

        readonly property point endMarkerPoint: Qt.point(
            item.startPoint.x > item.endPoint.x ? markerOffset : markerRight,
            item.startPoint.y > item.endPoint.y ? markerOffset : markerBottom)
    }
}
