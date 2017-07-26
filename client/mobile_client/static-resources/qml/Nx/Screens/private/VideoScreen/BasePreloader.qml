import QtQuick 2.6

Item
{
    id: control

    property alias timeout: timer.interval

    visible: false

    function hide(immediately)
    {
        if (immediately)
            control.visible = false
        else
            timer.start()
    }

    function show()
    {
        control.visible = true
        timer.stop()
    }

    Timer
    {
        id: timer

        interval: 1500
        onTriggered: control.visible = false
    }
}
