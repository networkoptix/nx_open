import QtQuick 2.0
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Dialogs 1.0

LabeledItem
{
    id: control

    property var value: ""
    property var defaultValue: ""
    property int maxPoints: -1

    property string figureType: ""

    contentItem: Row
    {
        spacing: 8

        Item
        {
            implicitWidth: 120
            implicitHeight: 80

            Image
            {
                id: backgroundImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit

                Connections
                {
                    target: thumbnailProvider

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
                anchors.fill: parent
                visible: hasFigure
            }

            Rectangle
            {
                id: noShapeDummy

                anchors.fill: parent
                visible: !preview.hasFigure

                color: "transparent"
                border.color: ColorTheme.colors.dark11
                border.width: 1

                Column
                {
                    anchors.centerIn: parent

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
            }

            MouseArea
            {
                anchors.fill: parent
                onClicked:
                {
                    dialogLoader.active = true
                    var dialog = dialogLoader.item
                    dialog.deserializeFigure(value)
                    dialog.show()
                }
            }
        }

        Button
        {
            padding: 0
            iconUrl: "qrc:/skin/buttons/delete.png"
            onClicked: value = ""
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
                maxPolygonPoints: maxPoints
                resourceId: settingsView.resourceId

                onAccepted:
                {
                    value = serializeFigure()
                }
            }
        }
    }

    onValueChanged:
    {
        var obj = null
        if (value)
        {
            obj = JSON.parse(JSON.stringify(value))
            obj.type = figureType
        }
        preview.figure = obj
    }
}
