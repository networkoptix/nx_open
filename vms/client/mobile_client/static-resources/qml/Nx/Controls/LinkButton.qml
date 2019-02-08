import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

Button
{
    id: control

    property color color: ColorTheme.brand_d2

    implicitHeight: 32
    implicitWidth: contentItem ? contentItem.implicitWidth : 0

    background: null
    contentItem: Text
    {
        text: control.text
        width: control.width
        height: control.height
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        color: control.color
        font.pixelSize: 15
        font.underline: true
    }

    MouseArea
    {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: control.clicked()
    }

    Component.onCompleted: control["activeFocusOnTab"] = false
}
