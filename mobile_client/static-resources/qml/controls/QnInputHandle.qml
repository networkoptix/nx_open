import QtQuick 2.4

Item {
    id: handle

    property color color: "red"

    property string anchor: "cursorPosition"
    property TextInput input
    property Flickable flickable

    readonly property alias pressed: mouseArea.pressed

    readonly property bool insideInputBounds: {
        var minX = flickable.contentX
        var maxX = flickable.x + flickable.width + flickable.contentX
        var ax = anchorX + flickable.contentX

        return ax >= minX && ax <= maxX
    }

    property real autoScrollMargin: dp(8)

    property real anchorShift: 0

    property real anchorX: {
        if (!input || !flickable)
            return 0

        var rect = input.positionToRectangle(input[anchor])
        return flickable.x - flickable.contentX + rect.x + rect.width / 2
    }

    implicitWidth: handleItem.width
    implicitHeight: handleItem.height

    x: anchorX - anchorShift
    y: parent.height

    Binding {
        target: handle
        property: "opacity"
        value: insideInputBounds ? 1.0 : 0.0
    }

    Behavior on opacity {
        enabled: handle.visible
        NumberAnimation { duration: 50 }
    }

    Image {
        id: handleItem
    }

    MouseArea {
        id: mouseArea

        property real pressX
        preventStealing: true

        anchors.fill: parent
        drag.minimumX: flickable.x - anchorShift
        drag.maximumX: flickable.x + flickable.width - anchorShift

        onPressed: pressX = mouse.x
        onMouseXChanged: updateAnchor(mouse.x)
        onReleased: autoScrollTimer.stop()
    }

    states: [
        State {
            name: "cursorPosition"
            when: handle.anchor == "cursorPosition"

            PropertyChanges {
                target: handleItem
                source: "image://icon/controls/cursor_handle.png"
            }
            PropertyChanges {
                target: handle
                implicitHeight: handleItem.height + handleItem.width / 4
                anchorShift: width / 2
            }
        },
        State {
            name: "selectionStart"
            when: handle.anchor == "selectionStart"

            PropertyChanges {
                target: handleItem
                source: "image://icon/controls/selection_start_handle.png"
            }
            PropertyChanges {
                target: handle
                implicitHeight: handleItem.height
                anchorShift: width
            }
        },
        State {
            name: "selectionEnd"
            when: handle.anchor == "selectionEnd"

            PropertyChanges {
                target: handleItem
                source: "image://icon/controls/selection_end_handle.png"
            }
            PropertyChanges {
                target: handle
                implicitHeight: handleItem.height
                anchorShift: 0
            }
        }
    ]

    Timer {
        id: autoScrollTimer
        interval: 20
        running: false
        repeat: true

        property int direction: 0

        onTriggered: {
            if (!input || !flickable)
                return

            if (anchor == "cursorPosition") {
                input.cursorPosition += direction
            } else if (anchor == "selectionStart") {
                var selectionStart = input.selectionStart + direction
                if (selectionStart >= 0 && selectionStart < input.selectionEnd) {
                    input.ensureVisible(selectionStart)
                    input.select(selectionStart, input.selectionEnd)
                }
            } else if (anchor == "selectionEnd") {
                var selectionEnd = input.selectionEnd + direction
                if (selectionEnd < input.text.length && selectionEnd > input.selectionStart) {
                    input.ensureVisible(selectionEnd)
                    input.select(input.selectionStart, selectionEnd)
                }
            }
        }
    }

    function updateAnchor(mouseX) {
        if (!input || !flickable)
            return

        var dx = mouseX - mouseArea.pressX

        var minX = flickable.contentX
        var maxX = flickable.x + flickable.width + flickable.contentX
        var ax = Math.max(minX, Math.min(maxX, anchorX + dx + flickable.contentX))

        var pos = input.positionAt(ax, input.height / 2)

        if (anchor == "cursorPosition") {
            input.cursorPosition = pos
        } else if (anchor == "selectionStart") {
            input.select(Math.min(input.selectionEnd - 1, pos), input.selectionEnd)
        } else if (anchor == "selectionEnd") {
            input.select(input.selectionStart, Math.max(input.selectionStart + 1, pos))
        }

        if (ax == minX) {
            autoScrollTimer.direction = -1
            autoScrollTimer.start()
        } else if (ax == maxX) {
            autoScrollTimer.direction = 1
            autoScrollTimer.start()
        } else {
            autoScrollTimer.stop()
        }
    }
}

