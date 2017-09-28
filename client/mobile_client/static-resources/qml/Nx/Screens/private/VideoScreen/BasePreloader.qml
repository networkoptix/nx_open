import QtQuick 2.6
import QtGraphicalEffects 1.0;
import Qt.labs.templates 1.0

Control
{
    id: control

    property alias timeout: timer.interval

    visible: opacity > 0
    opacity: 0

    onContentItemChanged:
    {
        contentItem.visible = false
    }
    background: DropShadow
    {
        id: shadow

        anchors.fill: parent
        source: contentItem
        color: Qt.rgba(0, 0, 0, 0.2)
    }

    state: "invisible"

    states: [
        State
        {
            name: "visible"
            PropertyChanges
            {
                target: control
                opacity: 1
            }
        },
        State
        {
            name: "invisible"
            PropertyChanges
            {
                target: control
                opacity: 0
            }
        }
    ]

    transitions: [
        Transition
        {
            from: "visible"
            to: "invisible"

            NumberAnimation
            {
                target: control
                property: "opacity"
                duration: 500
                easing.type: Easing.OutQuad
            }
        },
        Transition
        {
            from: "invisible"
            to: "visible"

            NumberAnimation
            {
                target: control
                property: "opacity"
                duration: 200
                easing.type: Easing.InQuad
            }
        }
    ]

    function hide(immediately)
    {
        if (immediately)
            control.state = "invisible"
        else
            timer.restart()
    }

    function show()
    {
        control.state = "visible"
        timer.stop()
    }

    Timer
    {
        id: timer

        interval: 1500
        onTriggered:
        {
            control.state = "invisible"
        }
    }
}
