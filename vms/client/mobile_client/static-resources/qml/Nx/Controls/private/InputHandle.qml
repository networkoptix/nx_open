import QtQuick 2.6

Item
{
    id: handle

    property string anchor: "cursorPosition"
    property TextInput input: null

    property bool autoHide: false

    readonly property alias pressed: mouseArea.pressed

    readonly property bool insideInputBounds:
    {
        var minX = input.leftPadding
        var maxX = input.width - input.rightPadding + 1
        var ax = anchorX

        return ax >= minX && ax <= maxX
    }

    property real autoScrollMargin: 8

    property real anchorShift: 0

    property real anchorX:
    {
        if (!input)
            return 0

        var rect = input.positionToRectangle(input[anchor])
        return rect.x + rect.width / 2 + control.leftPadding
    }

    implicitWidth: handleItem.width
    implicitHeight: handleItem.height

    property real localX: anchorX - anchorShift
    property real localY: input.cursorRectangle.y + input.cursorRectangle.height

    x: localX
    y: localY

    visible: false

    Binding
    {
        target: handle
        property: "opacity"
        value: insideInputBounds ? 1.0 : 0.0
    }

    Behavior on opacity
    {
        enabled: handle.visible
        NumberAnimation { duration: 50 }
    }

    Image { id: handleItem }

    MouseArea
    {
        id: mouseArea

        property real pressX
        preventStealing: true

        anchors.fill: parent
        drag.minimumX: input.leftPadding + anchorShift
        drag.maximumX: input.width - input.rightPadding - anchorShift

        onPressed: pressX = mouse.x
        onMouseXChanged: updateAnchor(mouse.x)
        onReleased: autoScrollTimer.stop()
    }

    Timer
    {
        running: autoHide && visible && opacity > 0 && !pressed
        interval: 5000
        onTriggered: opacity = 0.0
    }

    states:
    [
        State
        {
            name: "cursorPosition"
            when: handle.anchor == "cursorPosition"

            PropertyChanges
            {
                target: handleItem
                source: lp("/images/controls/cursor_handle.png")
            }
            PropertyChanges
            {
                target: handle
                implicitHeight: handleItem.height + handleItem.width / 4
                anchorShift: width / 2
            }
        },
        State
        {
            name: "selectionStart"
            when: handle.anchor == "selectionStart"

            PropertyChanges
            {
                target: handleItem
                source: lp("/images/controls/selection_start_handle.png")
            }
            PropertyChanges
            {
                target: handle
                implicitHeight: handleItem.height
                anchorShift: width
            }
        },
        State
        {
            name: "selectionEnd"
            when: handle.anchor == "selectionEnd"

            PropertyChanges
            {
                target: handleItem
                source: lp("/images/controls/selection_end_handle.png")
            }
            PropertyChanges
            {
                target: handle
                implicitHeight: handleItem.height
                anchorShift: 0
            }
        }
    ]

    Timer
    {
        id: autoScrollTimer
        interval: 20
        running: false
        repeat: true

        property int direction: 0

        onTriggered:
        {
            if (!input)
                return

            if (anchor == "cursorPosition")
            {
                input.cursorPosition += direction
            }
            else if (anchor == "selectionStart")
            {
                var selectionStart = input.selectionStart + direction
                if (selectionStart >= 0 && selectionStart < input.selectionEnd)
                {
                    input.ensureVisible(selectionStart)
                    input.select(selectionStart, input.selectionEnd)
                }
            }
            else if (anchor == "selectionEnd")
            {
                var selectionEnd = input.selectionEnd + direction
                if (selectionEnd < input.text.length && selectionEnd > input.selectionStart)
                {
                    input.ensureVisible(selectionEnd)
                    input.select(input.selectionStart, selectionEnd)
                }
            }
        }
    }

    function updateAnchor(mouseX)
    {
        if (!input)
            return

        var dx = mouseX - mouseArea.pressX

        var minX = input.leftPadding
        var maxX = input.width - input.rightPadding
        var ax = Math.max(minX, Math.min(maxX, anchorX + dx))

        var pos = input.positionAt(ax, input.height / 2)

        if (anchor == "cursorPosition")
            input.cursorPosition = pos
        else if (anchor == "selectionStart")
            input.select(Math.min(input.selectionEnd - 1, pos), input.selectionEnd)
        else if (anchor == "selectionEnd")
            input.select(input.selectionStart, Math.max(input.selectionStart + 1, pos))

        if (ax == minX)
        {
            autoScrollTimer.direction = -1
            autoScrollTimer.start()
        }
        else if (ax == maxX)
        {
            autoScrollTimer.direction = 1
            autoScrollTimer.start()
        }
        else
        {
            autoScrollTimer.stop()
        }
    }
}

