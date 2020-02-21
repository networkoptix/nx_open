import QtQuick 2.10

Item
{
    id: grip

    property bool ghost: false
    property color color: "black"
    property color ghostColor: "white"
    property color borderColor: "white"
    property int axis: Drag.XAndYAxis
    property bool hoverEnabled: true

    property alias hovered: mouseArea.containsMouse
    property alias pressed: mouseArea.pressed
    property alias cursorShape: mouseArea.cursorShape

    readonly property bool dragging: mouseArea.drag.active
    signal dragStarted()
    signal moved()
    signal clicked(int button)

    property real minX: 0
    property real minY: 0
    property real maxX: parent ? parent.width : 0
    property real maxY: parent ? parent.height : 0

    Rectangle
    {
        id: rect

        width: enabled && (mouseArea.containsMouse || mouseArea.pressed) ? 12 : 8
        height: width
        x: -width / 2
        y: -height / 2

        color: ghost ? ghostColor : grip.color
        border.color: ghost ? "transparent" : grip.borderColor
        border.width: 1

        opacity: ghost ? 0.7 : 1.0
    }

    MouseArea
    {
        id: mouseArea

        anchors.centerIn: rect
        width: 12
        height: 12

        acceptedButtons: Qt.LeftButton | Qt.RightButton
        hoverEnabled: grip.enabled && grip.hoverEnabled

        drag.minimumX: grip.minX
        drag.maximumX: grip.maxX
        drag.minimumY: grip.minY
        drag.maximumY: grip.maxY
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

            if (mouse.button === Qt.LeftButton)
                drag.target = grip
        }

        onReleased:
        {
            if (mouse.button === Qt.LeftButton)
                drag.target = undefined
        }

        onMouseXChanged: processMove(mouse)
        onMouseYChanged: processMove(mouse)

        onClicked: grip.clicked(mouse.button)

        function processMove(mouse)
        {
            if (dragging)
                moved()
        }
    }
}
