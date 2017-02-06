import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Button
{
    id: control

    flat: true
    textColor: ColorTheme.base1
    highlightColor: "#30000000"
    font.weight: Font.Normal

    implicitHeight: 48
    implicitWidth: parent ? parent.width / parent.children.length : 200
    radius: 0

    padding: 0
    leftPadding: 0
    rightPadding: 0

    property bool splitterVisible: parent && parent.children[parent.children.length - 1] !== this

    Rectangle
    {
        id: splitter

        height: 32
        width: 1
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: -0.5
        color: ColorTheme.contrast7
        visible: splitterVisible
    }
}
