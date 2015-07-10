import QtQuick 2.4
import QtQuick.Window 2.2

import "../main.js" as Main

FocusScope {
    id: popup

    property color overlayColor: "#80000000"
    property string overlayLayer
    property bool centered: false

    property real padding: dp(4)

    property Item _lastFocusedItem

    signal opened()
    signal closed()

    visible: false

    function open(x, y) {
        _lastFocusedItem = Window.activeFocusItem

        parent = Main.findRootChild(popup, overlayLayer)

        if (parent) {
            parent.color = overlayColor
            parent.show()
        }

        if (centered)
            positionAtCenter()
        else
            positionAt(x, y)

        opened()
        visible = true

        forceActiveFocus()
    }

    function positionAt(x, y) {
        var r = x + width + padding
        if (r > parent.width)
            x = Math.max(padding, x - (r - parent.width))

        var b = y + height + padding
        if (b > parent.height)
            y = Math.max(padding, y - (b - parent.height))

        popup.x = x
        popup.y = y
    }

    function positionAtCenter() {
        x = (parent.width - width) / 2
        y = (parent.height - height) / 2
    }

    function close() {
        if (!visible)
            return

        visible = false
        closed()

        parent.hide()

        _lastFocusedItem.forceActiveFocus()
    }

    Connections {
        ignoreUnknownSignals: true
        target: parent
        onDismiss: close()
    }

    focus: true

    Keys.onReleased: {
        if (event.key === Qt.Key_Back) {
            if (visible) {
                hide()
                event.accepted = true
            }
        }
    }
}

