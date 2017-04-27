import QtQuick 2.6

import "private" as Private

Rectangle
{
    id: control

    property bool supportsAutoFocus

    property bool focusInPressed: focusInButton.pressed
    property bool focusOutPressed: focusOutButton.pressed

    signal autoFocusClicked()

    color: "darkgrey" // TODO: set appropriate

    implicitWidth: 56
    implicitHeight: d.zoomButtonHeight * 2 + (supportsAutoFocus ? d.kAutoFocusButtonHeight : 0)

    radius: 28

    Column
    {
        spacing: 0
        anchors.fill: parent

        Private.RoundButton
        {
            id: focusInButton

            width: control.width
            height: d.zoomButtonHeight

            Text
            {
                anchors.centerIn: parent
                text: "+"
            }
        }

        Private.RoundButton
        {
            width: control.width
            height: d.kAutoFocusButtonHeight

            visible: control.supportsAutoFocus

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
            }

            onClicked: { control.autoFocusClicked() }
        }

        Private.RoundButton
        {
            id: focusOutButton

            width: control.width
            height: d.zoomButtonHeight

            Text
            {
                anchors.centerIn: parent
                text: "-"
            }
        }
    }

    QtObject
    {
        id: d

        readonly property int kAutoFocusButtonHeight: 32
        property int zoomButtonHeight: control.supportsAutoFocus ? 52 : 56
    }

}
