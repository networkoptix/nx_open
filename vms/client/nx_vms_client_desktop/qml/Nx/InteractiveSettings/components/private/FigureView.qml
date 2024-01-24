// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import Qt5Compat.GraphicalEffects

import Nx
import Nx.Core
import Nx.Controls
import Nx.Core.Items
import Nx.Items

import nx.vms.client.core
import nx.vms.client.desktop

/**
 * Interactive Settings private type.
 */
Item
{
    id: figureView

    property alias figure: preview.figure
    property alias figureType: preview.figureType
    property alias hasFigure: preview.hasFigure
    property alias backgroundColor: background.color
    property alias hovered: mouseArea.containsMouse
    property alias pressed: mouseArea.containsPress
    property bool editable: true

    readonly property var thumbnailSource: sharedData.thumbnailSource
    property int thumbnailReloadIntervalSeconds: 10

    signal editRequested()

    readonly property size implicitSize:
    {
        const rotated = Geometry.isRotated90(videoPositioner.videoRotation)
        const defaultSize = rotated ? Qt.size(100, 177) : Qt.size(177, 100)

        if (backgroundImage.status !== Image.Ready)
            return defaultSize

        const maxSize = Qt.size(177, 177)
        const imageSize = rotated
            ? Qt.size(backgroundImage.implicitHeight, backgroundImage.implicitWidth)
            : Qt.size(backgroundImage.implicitWidth, backgroundImage.implicitHeight)

        return Geometry.bounded(imageSize, maxSize)
    }

    implicitWidth: implicitSize.width
    implicitHeight: implicitSize.height

    activeFocusOnTab: editable

    VideoPositioner
    {
        id: videoPositioner

        anchors.fill: parent
        item: backgroundImage
        sourceSize: Qt.size(
            backgroundImage.implicitWidth, backgroundImage.implicitHeight)

        visible: preview.hasFigure
        videoRotation: thumbnailSource ? thumbnailSource.rotationQuadrants * 90.0 : 0

        layer.enabled: true
        layer.effect: OpacityMask
        {
            maskSource: Rectangle
            {
                parent: background
                radius: background.radius
                visible: false
                anchors.fill: parent
            }
        }

        Image
        {
            id: backgroundImage

            cache: false
            source: thumbnailSource ? thumbnailSource.url : ""

            Timer
            {
                id: updateTimer

                interval: thumbnailReloadIntervalSeconds * 1000
                running: thumbnailSource && thumbnailSource.active

                onTriggered:
                    thumbnailSource.update()
            }

            Rectangle
            {
                id: dimmer

                anchors.fill: parent
                color: ColorTheme.transparent("black",
                    mouseArea.containsMouse && !mouseArea.pressed ? 0.4 : 0.5)
            }
        }

        Rectangle
        {
            id: noDataIndicator

            color: ColorTheme.colors.dark6
            anchors.fill: preview
            visible: hasFigure && backgroundImage.status !== Image.Ready

            Text
            {
                id: noData

                anchors.fill: parent
                verticalAlignment: Qt.AlignVCenter
                horizontalAlignment: Qt.AlignHCenter
                color: ColorTheme.colors.light16
                font.pixelSize: 16
                font.weight: Font.Light
                fontSizeMode: Text.Fit
                padding: 4
                text: qsTr("NO PREVIEW")
            }
        }

        FigurePreview
        {
            id: preview

            readonly property size size:
            {
                const rotated = Geometry.isRotated90(videoPositioner.videoRotation)
                return rotated
                    ? Qt.size(parent.height, parent.width)
                    : Qt.size(parent.width, parent.height)
            }

            width: size.width
            height: size.height
            rotation: videoPositioner.videoRotation
            anchors.centerIn: parent
        }
    }

    Rectangle
    {
        id: background

        anchors.fill: parent
        visible: !preview.hasFigure || backgroundImage.status !== Image.Ready

        border.color: ColorTheme.colors.dark11
        border.width: 1
        radius: 2

        color:
        {
            if (!mouseArea.containsMouse)
                return "transparent"

            const baseColor = ColorTheme.lighter(ColorTheme.mid, mouseArea.pressed ? 0 : 2)
            return ColorTheme.transparent(baseColor, 0.2)
        }
    }

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        hoverEnabled: true
        enabled: editable

        onClicked:
            editRequested()
    }

    FocusFrame
    {
        id: focusMarker

        anchors.fill: parent
        visible: figureView.activeFocus
        color: ColorTheme.transparent(ColorTheme.highlight, 0.5)
        frameWidth: 1
    }

    // For some reason QML emits "changed" signal every time when a `var` property is assigned
    // (even to the same value). Thus, every time the plugin settings are reset, and values are
    // re-assigned to all of the FigureView items, the handler below is invoked and thumbnail is
    // re-requested forcefully. There's a simple workaround: store the figure value in a typed
    // property and subscribe to its changes. This works as expected: the handler runs only when
    // the value is really changed.
    property string figureStringified: figure ? JSON.stringify(figure) : ""
    onFigureStringifiedChanged:
    {
        if (thumbnailSource)
            thumbnailSource.update(/*forceRefresh*/ !!figure)

        if (updateTimer.running)
            updateTimer.restart()
    }

    Keys.enabled: editable

    Keys.onSpacePressed:
        editRequested()
}
