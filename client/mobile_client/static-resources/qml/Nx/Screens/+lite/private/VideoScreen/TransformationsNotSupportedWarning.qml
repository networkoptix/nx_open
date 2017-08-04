import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    signal closeClicked()

    Rectangle
    {
        anchors.fill: parent
        color: ColorTheme.transparent(ColorTheme.base1, 0.7)
    }

    Text
    {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height)

        text: qsTr("Software image rotation is not supported in fullscreen mode")
        color: ColorTheme.windowText

        font.pixelSize: 40

        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
    }

    Column
    {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 80
        spacing: 16

        Button
        {
            anchors.horizontalCenter: parent.horizontalCenter

            text: qsTr("Show as is")
            font.pixelSize: 28
            font.weight: Font.Normal

            height: 64
            labelPadding: 32
            radius: 4

            color: ColorTheme.transparent(ColorTheme.contrast4, 0.15)
            hoveredColor: ColorTheme.transparent(ColorTheme.contrast4, 0.2)

            onClicked: closeClicked()

            // Focused button also emits clicked() when <space> is pressed.
            Component.onCompleted: forceActiveFocus()
        }

        Text
        {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTr("(Space)")
            font.pixelSize: 24
            color: ColorTheme.contrast10
        }
    }
}
