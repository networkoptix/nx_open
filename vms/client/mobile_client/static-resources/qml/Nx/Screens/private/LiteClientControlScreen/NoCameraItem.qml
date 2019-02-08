import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    id: noCameraItem

    property bool active: false

    signal clicked()

    Rectangle
    {
        anchors.fill: parent
        anchors.margins: 3
        anchors.rightMargin: 16
        color: ColorTheme.base7

        Text
        {
            anchors.fill: parent
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("No image")
            font.capitalization: Font.AllUppercase
            font.pixelSize: 16
            elide: Text.ElideRight
            color: ColorTheme.base14
        }

        MaterialEffect
        {
            clip: true
            anchors.fill: parent
            mouseArea: mouseArea
            rippleSize: width * 2
        }

        MouseArea
        {
            id: mouseArea
            anchors.fill: parent
            onClicked: noCameraItem.clicked()
        }

        Rectangle
        {
            anchors.fill: parent
            anchors.margins: -4

            color: "transparent"
            border.color: ColorTheme.brand_main
            border.width: 2
            radius: 2
            visible: noCameraItem.active
        }
    }
}
