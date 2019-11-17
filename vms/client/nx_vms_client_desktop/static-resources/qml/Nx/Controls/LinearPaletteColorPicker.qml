import QtQuick 2.0
import Nx 1.0

Item
{
    id: colorPicker

    property alias colors: colorsRepeater.model
    property color color

    implicitWidth: row.implicitWidth
    implicitHeight: row.implicitHeight

    Row
    {
        id: row
        spacing: 4

        Repeater
        {
            id: colorsRepeater

            Rectangle
            {
                readonly property bool current: colorPicker.color === color

                width: 24
                height: 24
                radius: 2
                border.width: 1
                border.color: current ? ColorTheme.colors.light1 : "transparent"
                color: modelData

                Image
                {
                    anchors.centerIn: parent
                    source: "qrc:/skin/buttons/ok.png"
                    visible: parent.current
                }

                MouseArea
                {
                    anchors.fill: parent
                    onClicked: colorPicker.color = color
                }
            }
        }
    }
}
