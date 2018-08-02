import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0

Popup
{
    id: control

    background: Rectangle { color: ColorTheme.transparent(ColorTheme.base1, 0.9) }

    contentItem: Item
    {
        anchors.fill: parent

        Row
        {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: -56
            spacing: 56

            Column
            {
                Image
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: lp("/images/lite_client/selection_move.png")
                }
                Text
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Select another cell")
                    color: ColorTheme.base14
                    font.pixelSize: 20
                }
            }
            Column
            {
                Image
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: lp("/images/lite_client/camera_change.png")
                }
                Text
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Switch camera in the cell")
                    color: ColorTheme.base14
                    font.pixelSize: 20
                }
            }
            Column
            {
                Image
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    source: lp("/images/lite_client/camera_fullscreen.png")
                }
                Text
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Enter or exit fullscreen")
                    color: ColorTheme.base14
                    font.pixelSize: 20
                }
            }
        }

        Column
        {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 56

            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Press any key to go to cameras")
                color: ColorTheme.base11
                font.pixelSize: 20
            }
            Text
            {
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Press F1 to show this message again")
                color: ColorTheme.base11
                font.pixelSize: 20
            }
        }

        MouseArea
        {
            anchors.fill: parent
            onClicked: close()
        }

        Keys.onPressed:
        {
            close()
            event.accepted = true
        }
    }

    enter: Transition
    {
        NumberAnimation
        {
            property: "opacity"
            duration: 200
            easing.type: Easing.OutCubic
            to: 1.0
        }
    }

    exit: Transition
    {
        NumberAnimation
        {
            property: "opacity"
            duration: 200
            easing.type: Easing.OutCubic
            to: 0.0
        }
    }
}
