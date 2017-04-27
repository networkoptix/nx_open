import QtQuick 2.6

import "private" as Private

Private.GenericValueControl
{
    id: control

    property alias supportsAutoFocus: control.showCenterArea
    property alias focusInPressed: control.upPressed
    property alias focusOutPressed: control.downPressed

    actionButtonHeight: control.supportsAutoFocus ? 52 : 56

    signal autoFocusClicked()

    upButtonDecoration: Text
    {
        anchors.centerIn: parent
        color: "white"
        text: "F+"
    }

    downButtonDecoration: Text
    {
        anchors.centerIn: parent
        color: "white"
        text: "F-"
    }

    centralAreaDelegate: Private.RoundButton
    {
        width: control.width
        height: 32

        Rectangle
        {
            x: 4
            width: parent.width - x * 2
            height: parent.height

            radius: 8
            visible: !control.focusInPressed && !control.focusOutPressed
        }

        Text
        {
            text: qsTr("AF")
            anchors.centerIn: parent
            color: control.focusInPressed || control.focusOutPressed ? "white" : "black"
        }

        onClicked: { control.autoFocusClicked() }
    }
}
