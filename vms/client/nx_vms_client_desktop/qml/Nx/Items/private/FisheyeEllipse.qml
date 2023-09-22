// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Shapes 1.14

import Nx 1.0
import Nx.Core 1.0

import nx.vms.api 1.0

/**
 * A visual component displaying fisheye calibration ellipse and optional grid.
 */
Item
{
    id: fisheyeEllipse

    /** Relative center. */
    property real centerX: 0.5
    property real centerY: 0.5

    /** Relative horizontal radius. */
    property real radius: 0.4

    /** Absolute horizontal to vertical radius ratio. */
    property real aspectRatio: 1.5

    /** Fisheye lens projection type. */
    property int lensProjection: { return Dewarping.CameraProjection.equidistant }

    /** Camera roll correction angle. */
    property real rollCorrectionDegrees: 0

    /** Calibration ellipse display parameters. */
    property real lineWidth: 4
    property color lineColor: ColorTheme.colors.ptz.main

    // TODO: Shouldn't we use ColorTheme.colors.ptz.fill here?
    property color fillColor: ColorTheme.transparent(lineColor, 0.3)

    /** Grid parameters. */
    property bool showGrid: true
    property int longitudeStepDegrees: 15
    property int latitudeStepDegrees: 15

    Item
    {
        // Enable multisampling.
        layer.enabled: true
        layer.samples: 8

        // Rounding is required for good layer mapping.
        width: Math.round(parent.width)
        height: Math.round(parent.height)

        // Calibration ellipse and its center.
        Shape
        {
            id: shape
            anchors.fill: parent

            ShapePath
            {
                id: ellipsePath

                strokeWidth: fisheyeEllipse.lineWidth
                strokeColor: fisheyeEllipse.lineColor

                fillColor: fisheyeEllipse.fillColor

                startX: d.center.x - d.radii.x
                startY: d.center.y

                PathArc
                {
                    relativeX: d.radii.x * 2
                    relativeY: 0
                    radiusX: d.radii.x
                    radiusY: d.radii.y
                }

                PathArc
                {
                    relativeX: -d.radii.x * 2
                    relativeY: 0
                    radiusX: d.radii.x
                    radiusY: d.radii.y
                }
            }

            ShapePath
            {
                id: centerPath

                strokeWidth: fisheyeEllipse.lineWidth
                strokeColor: fisheyeEllipse.lineColor

                fillColor: "transparent"

                startX: d.center.x - fisheyeEllipse.lineWidth
                startY: d.center.y

                PathArc
                {
                    relativeX: fisheyeEllipse.lineWidth * 2
                    relativeY: 0
                    radiusX: fisheyeEllipse.lineWidth
                    radiusY: fisheyeEllipse.lineWidth
                }

                PathArc
                {
                    relativeX: -lineWidth * 2
                    relativeY: 0
                    radiusX: fisheyeEllipse.lineWidth
                    radiusY: fisheyeEllipse.lineWidth
                }
            }
        }

        // Longitude grid lines.
        Repeater
        {
            model: fisheyeEllipse.showGrid
                ? (360 / fisheyeEllipse.longitudeStepDegrees)
                : 1 //< Only the zero-angle marker line.

            Shape
            {
                id: longitudeLine
                anchors.fill: parent

                readonly property real angle: MathUtils.toRadians(90 - rollCorrectionDegrees
                    + index * fisheyeEllipse.longitudeStepDegrees)

                readonly property point xy: Qt.point(Math.cos(angle), Math.sin(angle))

                ShapePath
                {
                    strokeWidth: index == 0 ? fisheyeEllipse.lineWidth : 1
                    strokeColor: fisheyeEllipse.lineColor

                    fillColor: "transparent"

                    startX: d.center.x + fisheyeEllipse.lineWidth * longitudeLine.xy.x
                    startY: d.center.y - fisheyeEllipse.lineWidth * longitudeLine.xy.y

                    PathLine
                    {
                        x: d.center.x + d.radii.x * longitudeLine.xy.x
                        y: d.center.y - d.radii.y * longitudeLine.xy.y
                    }
                }
            }
        }

        // Latitude grid lines.
        Repeater
        {
            model: fisheyeEllipse.showGrid
                ? ((90 / fisheyeEllipse.latitudeStepDegrees) - 1)
                : 0

            Shape
            {
                id: latitudeEllipse
                anchors.fill: parent

                readonly property real latitudeRadiusFraction:
                {
                    const degreesFromPole = (index + 1) * fisheyeEllipse.latitudeStepDegrees

                    switch (fisheyeEllipse.lensProjection)
                    {
                        case Dewarping.CameraProjection.stereographic:
                            return Math.tan(MathUtils.toRadians(degreesFromPole) / 2.0)

                        case Dewarping.CameraProjection.equisolid:
                            return Math.sin(MathUtils.toRadians(degreesFromPole) / 2.0) * Math.sqrt(2.0)

                        default: // Dewarping.CameraProjection.equidistant
                            return degreesFromPole / 90.0;
                    }
                }

                readonly property point radii: Qt.point(d.radii.x * latitudeRadiusFraction,
                    d.radii.y * latitudeRadiusFraction)

                ShapePath
                {
                    strokeWidth: 1
                    strokeColor: fisheyeEllipse.lineColor

                    fillColor: "transparent"

                    startX: d.center.x - latitudeEllipse.radii.x
                    startY: d.center.y

                    PathArc
                    {
                        relativeX: latitudeEllipse.radii.x * 2
                        relativeY: 0
                        radiusX: latitudeEllipse.radii.x
                        radiusY: latitudeEllipse.radii.y
                    }

                    PathArc
                    {
                        relativeX: -latitudeEllipse.radii.x * 2
                        relativeY: 0
                        radiusX: latitudeEllipse.radii.x
                        radiusY: latitudeEllipse.radii.y
                    }
                }
            }
        }

        QtObject
        {
            id: d

            // Absolute center.
            property point center: Qt.point(
                fisheyeEllipse.centerX * fisheyeEllipse.width,
                fisheyeEllipse.centerY * fisheyeEllipse.height)

            // Absolute radii.
            property point radii: Qt.point(
                fisheyeEllipse.radius * fisheyeEllipse.width,
                fisheyeEllipse.radius * fisheyeEllipse.width / fisheyeEllipse.aspectRatio)
        }
    }
}
