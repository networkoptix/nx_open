import QtQuick 2.4
import QtQuick.Window 2.2

import "../main.js" as Main

FocusScope {
    id: popup

    property var showAnimation
    property var hideAnimation
    property bool hideLayerAfterAnimation: true

    QtObject {
        id: d
        property Item lastFocusedItem

        function finalizeHide() {
            visible = false

            if (hideLayerAfterAnimation)
                parent.hide()

            popup.hidden()

            d.lastFocusedItem.forceActiveFocus()
        }
    }

    signal shown()
    signal hidden()

    visible: false

    Connections {
        target: showAnimation ? showAnimation : null
        ignoreUnknownSignals: true
        onStopped: popup.shown()
    }

    Connections {
        target: hideAnimation ? hideAnimation : null
        ignoreUnknownSignals: true
        onStopped: d.finalizeHide()
    }

    function show() {
        if (!parent)
            return

        d.lastFocusedItem = Window.activeFocusItem

        parent.show()
        popup.visible = true

        if (showAnimation)
            showAnimation.start()
        else
            popup.shown()
    }

    function hide() {
        if (!parent || !visible)
            return

        if (hideAnimation)
            hideAnimation.start()
        else
            d.finalizeHide()
    }

    Connections {
        ignoreUnknownSignals: true
        target: parent
        onCloseRequested: hide()
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

