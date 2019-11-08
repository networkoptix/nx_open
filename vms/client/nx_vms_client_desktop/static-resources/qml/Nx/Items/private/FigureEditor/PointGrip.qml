import QtQuick 2.10

Item
{
    id: grip

    property bool ghost: false
    property color color: "black"
    property color borderColor: "white"
    property int axis: Drag.XAndYAxis
    property bool hoverEnabled: true

    property alias hovered: mouseArea.containsMouse
    property alias pressed: mouseArea.pressed
    property alias cursorShape: mouseArea.cursorShape

    readonly property bool dragging: mouseArea.drag.active
    signal dragStarted()
    signal moved()

    Rectangle
    {
        id: rect

        width: enabled && (mouseArea.containsMouse || mouseArea.pressed) ? 12 : 8
        height: width
        x: -width / 2
        y: -height / 2

        color: grip.color
        border.color: ghost ? "transparent" : grip.borderColor
        border.width: 1

        opacity: ghost ? 0.5 : 1.0
    }

    MouseArea
    {
        id: mouseArea

        anchors.centerIn: rect
        width: 12
        height: 12

        acceptedButtons: Qt.LeftButton
        hoverEnabled: grip.enabled && grip.hoverEnabled

        drag.minimumX: 0
        drag.maximumX: grip.parent ? grip.parent.width : 0
        drag.minimumY: 0
        drag.maximumY: grip.parent ? grip.parent.height : 0
        drag.axis: grip.axis
        drag.threshold: 0

        drag.onActiveChanged:
        {
            if (drag.active)
                grip.dragStarted()
        }

        onPressed:
        {
            if (!grip.parent)
                return

            drag.target = grip
        }

        onReleased:
        {
            drag.target = undefined
        }

        onMouseXChanged: processMove(mouse)
        onMouseYChanged: processMove(mouse)

        function processMove(mouse)
        {
            if (dragging)
                moved()
        }
    }
}
