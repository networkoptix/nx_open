// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Layouts 1.11
import QtQuick.Shapes 1.14
import Qt5Compat.GraphicalEffects

import Nx 1.0
import Nx.Core 1.0
import Nx.Items 1.0
import Nx.Controls 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

/**
 * 360VR panorama dewarping settings.
 */
Item
{
    id: settings

    /** Preview image source. */
    property AbstractResourceThumbnail previewSource: null

    /** Horizon correction angles. */
    property real alphaDegrees: 0.0
    property real betaDegrees: 0.0

    implicitHeight: content.implicitHeight
    implicitWidth: content.implicitWidth
    opacity: enabled ? 1.0 : 0.3

    ColumnLayout
    {
        id: content
        spacing: 16

        anchors.fill: parent

        Rectangle
        {
            id: previewArea

            Layout.fillWidth: true
            Layout.fillHeight: true

            color: ColorTheme.colors.dark4

            ResourcePreview
            {
                id: preview
                anchors.fill: parent

                source: settings.previewSource
                applyRotation: false

                loadingIndicatorDotSize: 16
                textSize: 24
            }

            Item
            {
                id: correctionArea

                parent: preview.imageArea
                anchors.fill: parent

                HorizonCorrection
                {
                    id: horizon

                    anchors.fill: parent
                    alphaDegrees: settings.alphaDegrees
                    betaDegrees: settings.betaDegrees

                    Rectangle
                    {
                        id: draggableMarker

                        width: 24
                        height: width
                        radius: width * 0.5

                        color: ColorTheme.colors.brand_d2
                        border.color: ColorTheme.colors.brand_core
                        border.width: 4

                        x: (alphaSpinBox.position * parent.width) - (width * 0.5)
                        y: ((1.0 - betaSpinBox.position) * parent.height) - (height * 0.5)

                        // Mouse drag implementation.
                        MouseArea
                        {
                            id: mouseArea
                            width: parent.width
                            height: parent.height

                            hoverEnabled: true

                            onPositionChanged:
                            {
                                if (!pressed)
                                    return

                                const pos = mapToItem(horizon, mouse.x, mouse.y)

                                settings.alphaDegrees = MathUtils.bound(-180.0,
                                    originalAngles.x + (pos.x - pressPos.x) / horizon.width * 360.0,
                                    180.0)

                                settings.betaDegrees = MathUtils.bound(-90.0,
                                    originalAngles.y - (pos.y - pressPos.y) / horizon.height * 180.0,
                                    90.0)
                            }

                            onPressed:
                            {
                                pressPos = mapToItem(horizon, mouse.x, mouse.y)
                                originalAngles = Qt.vector2d(settings.alphaDegrees,
                                    settings.betaDegrees)
                            }

                            property point pressPos
                            property vector2d originalAngles

                            CursorOverride.active: containsMouse || pressed

                            CursorOverride.shape: pressed
                                ? Qt.ClosedHandCursor
                                : Qt.OpenHandCursor
                        }
                    }
                }
            }
        }

        RowLayout
        {
            id: controlsLayout

            Layout.alignment: Qt.AlignTop
            Layout.fillWidth: true
            spacing: 32

            Column
            {
                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                spacing: 8

                Label { text: qsTr("Horizon correction") }

                GridLayout
                {
                    columns: 2
                    rowSpacing: 8
                    columnSpacing: 8

                    Label
                    {
                        text: "\u03B1" //< Greek alpha.
                    }

                    DoubleSpinBox
                    {
                        id: alphaSpinBox

                        dFrom: -180.0
                        dTo: 180.0
                        dStepSize: 1.0
                        decimals: 1
                        editable: true
                        suffix: " \u00B0" //< Degree sign.

                        Layout.fillWidth: true

                        onValueModified:
                            settings.alphaDegrees = dValue

                        Connections
                        {
                            target: settings

                            function onAlphaDegreesChanged()
                            {
                                alphaSpinBox.dValue = MathUtils.visualNormalized180(
                                    settings.alphaDegrees)
                            }
                        }
                    }

                    Label
                    {
                        text: "\u03B2" //< Greek beta.
                    }

                    DoubleSpinBox
                    {
                        id: betaSpinBox

                        dFrom: -90.0
                        dTo: 90.0
                        dStepSize: 1.0
                        decimals: 1
                        editable: true
                        suffix: " \u00B0" //< Degree sign.

                        onValueModified:
                            settings.betaDegrees = dValue

                        Layout.fillWidth: true

                        Connections
                        {
                            target: settings

                            function onBetaDegreesChanged()
                            {
                                betaSpinBox.dValue = MathUtils.visualNormalized180(
                                    settings.betaDegrees)
                            }
                        }
                    }
                }
            }

            TextButton
            {
                id: resetButton

                text: qsTr("Reset")
                icon.source: "image://svg/skin/text_buttons/reload_20.svg"
                enabled: settings.betaDegrees || settings.alphaDegrees

                Layout.alignment: Qt.AlignTop

                onClicked:
                {
                    settings.betaDegrees = 0
                    settings.alphaDegrees = 0
                }
            }
        }
    }
}
