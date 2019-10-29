import QtQuick 2.11
import QtQuick.Window 2.0
import Nx 1.0
import Nx.Items 1.0
import Nx.Controls 1.0

Window
{
    modality: Qt.WindowModal

    width: 600
    height: 400

    color: ColorTheme.colors.dark7

    signal accepted()

    property alias figureType: editor.figureType
    property int maxPolygonPoints: -1

    property alias resourceId: editor.resourceId

    Item
    {
        width: parent.width
        height: parent.height - bottomPanel.height

        FigureEditor
        {
            id: editor

            anchors.fill: parent
        }

        Rectangle
        {
            visible: !editor.hasFigure
            anchors.bottom: parent.bottom
            width: parent.width
            height: 32
            color: ColorTheme.colors.brand_d6

            Text
            {
                anchors.fill: parent
                verticalAlignment: Text.AlignVCenter
                leftPadding: 16
                color: ColorTheme.buttonText
                text:
                {
                    if (figureType === "line")
                        return qsTr("Click on screen to add line")
                    else if (figureType === "box")
                        return qsTr("Click on screen to add box")
                    else if (figureType === "polygon")
                        return qsTr("Click on screen to add polygon")
                    return ""
                }
            }
        }
    }

    Item
    {
        id: bottomPanel

        anchors.bottom: parent.bottom
        width: parent.width
        height: 58

        Row
        {
            height: parent.height
            x: 16
            spacing: 16

            LinearPaletteColorPicker
            {
                anchors.verticalCenter: parent.verticalCenter

                colors: [
                    "#e040fb",
                    "#536dfe",
                    "#64ffda",
                    "#ffff00",
                    "#ffab40",
                    "#b2ff59",
                    "#7c4dff",
                    "#ff4081"
                ]

                color: editor.color
                onColorChanged: editor.color = color
            }

            Button
            {
                anchors.verticalCenter: parent.verticalCenter
                flat: true
                text: qsTr("Reset")
                iconUrl: "qrc:/skin/text_buttons/refresh.png"
                leftPadding: 0
                rightPadding: 2
                onClicked: editor.clear()
            }
        }

        Row
        {
            spacing: 8
            anchors
            {
                right: parent.right
                rightMargin: 16
                verticalCenter: parent.verticalCenter
            }

            Button
            {
                text: qsTr("OK")
                isAccentButton: true
                onClicked:
                {
                    close()
                    accepted()
                }
                width: Math.max(implicitWidth, 80)
            }

            Button
            {
                text: qsTr("Cancel")
                onClicked: close()
                width: Math.max(implicitWidth, 80)
            }
        }
    }

    function serializeFigure()
    {
        return editor.serialize()
    }

    function deserializeFigure(json)
    {
        editor.deserialize(json)
    }
}
