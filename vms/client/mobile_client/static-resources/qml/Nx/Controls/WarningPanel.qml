import QtQuick 2.0
import QtQuick.Controls 2.4
import Nx 1.0

Pane
{
    id: control

    property string text
    property bool opened: false

    implicitWidth: parent ? parent.width : background.width
    implicitHeight: background.implicitHeight
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 16
    font.weight: Font.DemiBold
    clip: true

    height: opened ? implicitHeight : 0
    Behavior on height
    {
        NumberAnimation
        {
            duration: 300
            easing.type: Easing.OutCubic
        }
    }

    background: Rectangle
    {
        implicitWidth: 200
        implicitHeight: 40
        color: ColorTheme.red_main
    }

    contentItem: Text
    {
        text: control.text
        font: control.font
        height: control.implicitHeight
        width: control.availableWidth
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: ColorTheme.windowText
        elide: Text.ElideRight
    }
}
