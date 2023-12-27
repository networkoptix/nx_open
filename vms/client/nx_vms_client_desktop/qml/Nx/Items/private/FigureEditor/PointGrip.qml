// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.10

Item
{
    id: grip

    property bool ghost: false
    property alias color: marker.color
    property alias ghostColor: marker.ghostColor
    property alias borderColor: marker.borderColor
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

    PointMarker
    {
        id: marker

        hovered: enabled && (mouseArea.containsMouse || mouseArea.pressed)
        ghost: grip.ghost
    }

    MouseArea
    {
        id: mouseArea

        anchors.centerIn: marker
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

        onPressed: (mouse) =>
        {
            if (!grip.parent)
                return

            if (mouse.button === Qt.LeftButton)
                drag.target = grip
        }

        onReleased: (mouse) =>
        {
            if (mouse.button === Qt.LeftButton)
                drag.target = undefined
        }

        onMouseXChanged: (mouse) => processMove(mouse)
        onMouseYChanged: (mouse) => processMove(mouse)

        onClicked: (mouse) => grip.clicked(mouse.button)

        function processMove(mouse)
        {
            if (dragging)
                moved()
        }
    }
}
