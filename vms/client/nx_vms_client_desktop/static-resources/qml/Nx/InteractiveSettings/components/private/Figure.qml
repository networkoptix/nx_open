import QtQuick 2.0
import Nx 1.0
import Nx.Controls 1.0
import Nx.Items 1.0
import Nx.Dialogs 1.0

LabeledItem
{
    id: control

    property string value: ""
    property string defaultValue: ""
    property int maxPoints: -1

    property string figureType: ""

    contentItem: Row
    {
        spacing: 8

        Item
        {
            implicitWidth: 120
            implicitHeight: 80

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
                    dialog.deserializeFigure(getJsonValue())
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

                onAccepted:
                {
                    value = JSON.stringify(serializeFigure())
                }
            }
        }
    }

    function getJsonValue()
    {
        try
        {
            return JSON.parse(value)
        }
        catch (e)
        {
            if (value)
                console.warn(e.message)
            return null
        }
    }

    onValueChanged:
    {
        var obj = getJsonValue()
        if (obj)
            obj.type = figureType
        preview.figure = obj
    }
}
