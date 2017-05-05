import QtQuick 2.6
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Controls 1.0

Control
{
    id: control

    property bool active: false
    property alias text: label.text
    property alias textColor: label.color

    signal clicked()

    implicitWidth: parent ? parent.width : 200
    implicitHeight: 48
    leftPadding: 16
    rightPadding: 16

    font.pixelSize: 16

    background: Item
    {
        Rectangle
        {
            anchors.fill: parent
            visible: control.activeFocus || control.active
            color: control.active ? ColorTheme.brand_main : ColorTheme.brand_l2
        }

        MaterialEffect
        {
            anchors.fill: parent
            mouseArea: mouseArea
            clip: true
            highlightColor: "#30000000"
            rippleSize: 160
        }
    }

    MouseArea
    {
        id: mouseArea
        parent: control
        anchors.fill: control
        onClicked: control.clicked()
    }

    Text
    {
        id: label

        anchors.verticalCenter: parent.verticalCenter
        x: parent.leftPadding
        width: parent.availableWidth
        font: control.font
        color: control.active ? ColorTheme.brand_contrast : ColorTheme.base1
    }

    Keys.onEnterPressed: clicked()
    Keys.onReturnPressed: clicked()
}
