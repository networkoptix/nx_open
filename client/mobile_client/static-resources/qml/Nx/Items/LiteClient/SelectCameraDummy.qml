import QtQuick 2.6
import Nx 1.0

Rectangle
{
    color: ColorTheme.base3

    implicitWidth: content.width
    implicitHeight: content.height

    Column
    {
        id: content

        width: parent.width
        anchors.centerIn: parent
        spacing: 40

        Text
        {
            width: parent.width
            height: 80
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Select camera")
            color: ColorTheme.contrast10
            font.pixelSize: 64
            font.capitalization: Font.AllUppercase
            wrapMode: Text.WordWrap
        }

        Text
        {
            width: parent.width
            height: 80
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Press Ctrl + Arrow or use mouse wheel")
            color: ColorTheme.contrast10
            font.pixelSize: 30
            wrapMode: Text.WordWrap
        }
    }
}
