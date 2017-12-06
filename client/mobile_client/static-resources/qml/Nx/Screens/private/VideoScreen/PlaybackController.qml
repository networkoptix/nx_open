import QtQuick 2.6
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Controls 1.0

AbstractButton
{
    id: control

    property bool loading: false
    property color color: ColorTheme.windowText
    property bool paused: false

    implicitWidth: background.implicitWidth
    implicitHeight: background.implicitHeight

    background: Rectangle
    {
        implicitWidth: 56
        implicitHeight: 56

        color: ColorTheme.transparent(ColorTheme.base3, 0.2)
        radius: height / 2

    }

    label: Rectangle
    {
        anchors.fill: parent
        radius: height / 2
        color: "transparent"
        border.width: 2
        border.color: control.color

        CircleBusyIndicator
        {
            id: loadingIndicator

            anchors.fill: parent
            anchors.margins: 2
            running: control.loading
            opacity: control.loading ? 0.5 : 0.0
            visible: opacity > 0.0
            color: control.color
            lineWidth: 2

            Behavior on opacity
            {
                SequentialAnimation
                {
                    PauseAnimation { duration: loadingIndicator.visible ? 0 : 250 }
                    NumberAnimation { duration: 250 }
                }
            }
        }

        PlayPauseIcon
        {
            width: 18
            height: 18
            anchors.centerIn: parent
            pauseState: control.paused
            color: control.color
            opacity: control.loading ? 0.5 : 1.0
            Behavior on opacity { NumberAnimation { duration: 250 } }
        }
    }

    Rectangle
    {
        anchors.fill: parent
        radius: height / 2
        color: "black"
        opacity: control.pressed ? 0.2 : 0.0
    }
}
