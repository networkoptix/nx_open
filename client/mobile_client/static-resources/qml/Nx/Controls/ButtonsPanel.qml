import QtQuick 2.6

ListView
{
    id: control

    property int pressedStateFilterMs: 500

    signal buttonClicked(int index)
    signal pressedChanged(int index, bool pressed)

    clip: true
    layoutDirection: Qt.RightToLeft
    orientation: Qt.Horizontal
    implicitHeight: 48

    interactive: contentWidth > width

    Image
    {
        source: lp("/images/bottom_panel_shadow_left.png")
        anchors.left: parent.left
        visible: interactive && visibleArea.xPosition > 0
    }

    Image
    {
        source: lp("/images/bottom_panel_shadow_right.png")
        anchors.right: parent.right
        visible: interactive && (visibleArea.widthRatio + visibleArea.xPosition < 1)
    }

    delegate:
        IconButton
        {
            id: button

            icon: lp(model.iconPath)

            anchors.verticalCenter: parent.verticalCenter

            Connections
            {
                target: control
                onFlickingChanged:
                {
                    pressedStateFilterTimer.stop();
                    releasedStateFilterTimer.stop()
                }
                onDraggingChanged:
                {
                    pressedStateFilterTimer.stop();
                    releasedStateFilterTimer.stop()
                }
            }

            property bool filteringPressing: false

            onClicked:
            {
                if (!filteringPressing)
                {
                    pressedStateFilterTimer.stop()
                    releasedStateFilterTimer.stop()
                    control.buttonClicked(index)
                }
            }

            onPressedChanged:
            {
                if (pressed)
                {
                    releasedStateFilterTimer.stop()
                    pressedStateFilterTimer.restart()
                }
                else
                {
                    if (pressedStateFilterTimer.running)
                        pressedStateFilterTimer.stop()
                    else
                        releasedStateFilterTimer.restart();
                }
            }

            Timer
            {
                id: pressedStateFilterTimer
                interval: control.pressedStateFilterMs
                onTriggered: finishStateProcessing(true)
            }

            Timer
            {
                id: releasedStateFilterTimer
                interval: control.pressedStateFilterMs
                onTriggered: finishStateProcessing(false)
            }

            function finishStateProcessing(value)
            {
                button.filteringPressing = value
                control.pressedChanged(index, value)
            }
        }

}
