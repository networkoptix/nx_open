import QtQuick 2.0
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

    signal valueChanged()

    contentItem: Column
    {
        spacing: 12

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
                readonly property size implicitSize:
                {
                    const rotated = Geometry.isRotated90(videoPositioner.videoRotation)
                    const defaultSize = rotated ? Qt.size(80, 120) : Qt.size(120, 80)

                    if (backgroundImage.status !== Image.Ready)
                        return defaultSize

                    const maxSize = Qt.size(120, 120)
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

                    Image
                    {
                        id: backgroundImage

                        Connections
                        {
                            target: thumbnailProvider
                            ignoreUnknownSignals: true

                            onThumbnailUpdated:
                            {
                                if (cameraId.toString() === settingsView.resourceId.toString())
                                    backgroundImage.source = thumbnailUrl
                            }
                        }

                        Connections
                        {
                            target: settingsView

                            onResourceIdChanged:
                            {
                                if (!thumbnailProvider)
                                    return

                                if (settingsView.resourceId.isNull())
                                    return

                                thumbnailProvider.refresh(settingsView.resourceId)
                            }
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
                    id: border

                    anchors.fill: parent
                    visible: !preview.hasFigure || backgroundImage.status !== Image.Ready
                    color: "transparent"
                    border.color: ColorTheme.colors.dark11
                    border.width: 1

                }

                Column
                {
                    id: noShapeDummy

                    anchors.centerIn: parent
                    visible: !preview.hasFigure

                    Text
                    {
                        text: qsTr("No shape")
                        color: ColorTheme.colors.dark11
                        font.pixelSize: 11
                        anchors.horizontalCenter: parent.horizontalCenter
                    }

                    Text
                    {
                        text: qsTr("click to add")
                        color: ColorTheme.colors.dark11
                        font.pixelSize: 9
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }

                MouseArea
                {
                    anchors.fill: parent
                    onClicked: openEditDialog()
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
                    onClicked: figure = null
                    visible: preview.hasFigure
                    backgroundColor: ColorTheme.colors.red_core
                }
            }
        }

        CheckBox
        {
            id: showOnCameraCheckBox
            text: qsTr("Display on camera")
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
        // Our agreement is that figure without points is an invalid figure. It should not be sent
        // to server.
        if (!figure || !figure.points || figure.points.length === 0)
            return null

        // Clone the figure object and fill the rest of fields.
        var obj = JSON.parse(JSON.stringify(figure))
        obj.name = figureNameEdit.text
        obj.showOnCamera = showOnCameraCheckBox.checked
        return obj
    }

    function setValue(value)
    {
        figureNameEdit.text = (value && value.name) || ""
        showOnCameraCheckBox.checked = value && value.showOnCamera !== false
        figure = value
    }
}
