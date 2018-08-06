import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx 1.0
import Nx.Controls 1.0

AbstractButton
{
    id: control

    property bool loading: false
    property color color: ColorTheme.windowText
    property bool paused: false
    property color highlightColor: "#30ffffff"

    implicitWidth: background.implicitWidth
    implicitHeight: background.implicitHeight

    onPressAndHold: { d.pressedAndHeld = true }
    onCanceled: { d.pressedAndHeld = false }
    onReleased:
    {
        if (!d.pressedAndHeld)
            return

        d.pressedAndHeld = false
        clicked()
    }

    background: Rectangle
    {
        implicitWidth: 56
        implicitHeight: 56

        color: ColorTheme.transparent(ColorTheme.base1, 0.2)
        radius: height / 2

        MaterialEffect
        {
            anchors.fill: parent
            clip: true
            radius: parent.radius
            mouseArea: control
            rippleSize: 160
            highlightColor: control.highlightColor
        }
    }

    contentItem: Rectangle
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
        color: ColorTheme.transparent(ColorTheme.base1, 0.2)
        opacity: control.pressed ? 0.2 : 0.0
    }

    QtObject
    {
        id: d

        property bool pressedAndHeld: false
    }
}
