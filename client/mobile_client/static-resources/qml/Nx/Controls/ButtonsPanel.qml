import QtQuick 2.6

Item
{
    id: control

    property alias model: buttonRepeater.model
    property int pressedStateFilterMs: 200

    signal buttonClicked(int index)
    signal pressedChanged(int index, bool pressed)

    width: rowItem.width
    height: rowItem.height

    Row
    {
        id: rowItem

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        layoutDirection: Qt.RightToLeft
        Repeater
        {
            id: buttonRepeater

            delegate:
                IconButton
                {
                    id: button

                    icon: lp(model.iconPath)

                    anchors.verticalCenter: parent.verticalCenter

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
                            pressedStateFilterTimer.restart()
                        else
                            releasedStateFilterTimer.restart();
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
    }
}
