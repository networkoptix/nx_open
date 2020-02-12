import QtQuick 2.6
import QtGraphicalEffects 1.0

import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Dialogs 1.0
import Nx.Core.Items 1.0

import nx.client.core 1.0

LabeledItem
{
    id: control

    property string figureType: ""
    property var figureSettings
    property var figure

    isGroup: true

    signal valueChanged()

    contentItem: Column
    {
        spacing: 8

        baselineOffset: figureNameEdit.baselineOffset

        TextField
        {
            id: figureNameEdit
            width: parent.width
            onTextChanged: valueChanged()
        }

        Row
        {
            spacing: 8

            Item
            {
                id: figureBlock

                activeFocusOnTab: true

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

                VideoPositioner
                {
                    id: videoPositioner

                    anchors.fill: parent
                    item: backgroundImage
                    sourceSize: Qt.size(
                        backgroundImage.implicitWidth, backgroundImage.implicitHeight)

                    videoRotation: mediaResourceHelper ? mediaResourceHelper.customRotation : 0

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
                            target: thumbnailProvider
                            ignoreUnknownSignals: true

                            onThumbnailUpdated:
                            {
                                if (cameraId === settingsView.resourceId)
                                    backgroundImage.source = thumbnailUrl
                            }
                        }

                        Connections
                        {
                            target: settingsView
                            onResourceIdChanged: backgroundImage.updateThumbnail()
                        }

                        function updateThumbnail()
                        {
                            backgroundImage.source = ""

                            if (!thumbnailProvider)
                                return

                            if (settingsView.resourceId.isNull())
                                return

                            thumbnailProvider.refresh(settingsView.resourceId)
                        }

                        Component.onCompleted: updateThumbnail()

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

                        figure: control.figure
                        figureType: control.figureType
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

                Text
                {
                    text: qsTr("Click to add")
                    anchors.centerIn: parent
                    visible: !preview.hasFigure
                    font.pixelSize: 12
                    color: mouseArea.containsMouse
                        ? ColorTheme.colors.light14
                        : ColorTheme.colors.dark13
                }

                MouseArea
                {
                    id: mouseArea

                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: openEditDialog()
                }

                Rectangle
                {
                    id: focusMarker

                    anchors.fill: parent
                    radius: background.radius
                    visible: figureBlock.activeFocus
                    color: "transparent"
                    border.color: ColorTheme.transparent(ColorTheme.highlight, 0.5)
                    border.width: 1
                }

                Keys.onPressed:
                {
                    if (event.key == Qt.Key_Space)
                        openEditDialog()
                }
            }

            Column
            {
                spacing: 8

                Button
                {
                    text: qsTr("Edit")
                    onClicked: openEditDialog()
                }

                Button
                {
                    text: qsTr("Delete")
                    onClicked:
                    {
                        figure = null
                        figureNameEdit.text = ""
                    }
                    visible: preview.hasFigure
                    backgroundColor: ColorTheme.colors.red_core
                }
            }
        }

        CheckBox
        {
            id: showOnCameraCheckBox
            text: qsTr("Display on camera")
            enabled: preview.hasFigure
            onCheckedChanged: valueChanged()
            topPadding: 0
        }
    }

    Loader
    {
        id: dialogLoader

        active: false

        sourceComponent: Component
        {
            FigureEditorDialog
            {
                figureType: control.figureType
                figureSettings: control.figureSettings
                resourceId: settingsView.resourceId

                onAccepted: figure = serializeFigure()
            }
        }
    }

    onFigureChanged: valueChanged()

    function openEditDialog()
    {
        dialogLoader.active = true
        var dialog = dialogLoader.item
        dialog.title = figureNameEdit.text || qsTr("Figure")
        dialog.deserializeFigure(figure)
        dialog.show()
    }

    function getValue()
    {
        // Clone the figure object and fill the rest of fields.
        var obj = {
            "figure": figure ? JSON.parse(JSON.stringify(figure)) : null,
            "label": figureNameEdit.text,
            "showOnCamera": showOnCameraCheckBox.checked
        }
        return obj
    }

    function setValue(value)
    {
        figureNameEdit.text = (value && value.label) || ""
        showOnCameraCheckBox.checked = !value || value.showOnCamera !== false
        figure = value && value.figure
    }

    function resetValue()
    {
        setValue(null)
    }
}
