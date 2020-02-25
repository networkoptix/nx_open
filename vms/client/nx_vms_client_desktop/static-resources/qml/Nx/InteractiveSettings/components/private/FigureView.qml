import QtQuick 2.6
import QtGraphicalEffects 1.0

import Nx 1.0
import Nx.Controls 1.0
import Nx.Core.Items 1.0
import Nx.Items 1.0

import nx.client.core 1.0
import nx.client.desktop 1.0

Item
{
    id: figureView

    property alias figure: preview.figure
    property alias figureType: preview.figureType
    property alias hasFigure: preview.hasFigure
    property alias backgroundColor: background.color
    property alias hovered: mouseArea.containsMouse
    property alias pressed: mouseArea.containsPress

    property var resourceId
    property var resourceThumbnailProvider
    property alias previewRotation: videoPositioner.videoRotation

    property bool editable: true

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

            Connections
            {
                target: resourceThumbnailProvider || null
                ignoreUnknownSignals: true

                onThumbnailUpdated:
                {
                    if (cameraId === resourceId)
                        backgroundImage.source = thumbnailUrl
                }
            }

            function updateThumbnail()
            {
                backgroundImage.source = ""
                if (!resourceThumbnailProvider || !resourceId || resourceId.isNull())
                    return

                resourceThumbnailProvider.load(resourceId)
            }

            Component.onCompleted:
                updateThumbnail()

            Rectangle
            {
                id: dimmer

                anchors.fill: parent
                color: ColorTheme.transparent("black",
                    mouseArea.containsMouse && !mouseArea.pressed ? 0.4 : 0.5)
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

            var baseColor = ColorTheme.lighter(ColorTheme.mid, mouseArea.pressed ? 0 : 2)
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

    onResourceIdChanged:
        backgroundImage.updateThumbnail()

    Keys.enabled: editable

    Keys.onSpacePressed:
        editRequested()
}
