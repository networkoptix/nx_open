import QtQuick 2.4

import com.networkoptix.qml 1.0

QnPopup {
    id: menu

    parent: popupLayer

    x: d.x
    y: d.y
    opacity: 0.0
    hideLayerAfterAnimation: false

    QtObject {
        id: d

        property real x: 0
        property real y: 0

        readonly property real hMargin: dp(8)
    }

    function updateGeometry(button) {
        var buttonGeometry = parent.mapFromItem(button, 0, 0, button.width, button.height)
        var dx = parent.width / 2 - (buttonGeometry.x + buttonGeometry.width / 2)
        var dy = parent.height / 2 - (buttonGeometry.y + buttonGeometry.height / 2)

        if (dy >= 0) {
            d.y = buttonGeometry.y
        } else {
            var bottomMargin = parent.height - buttonGeometry.y - buttonGeometry.height
            d.y = Qt.binding(function() { return parent.height - bottomMargin - height })
        }

        if (dx >= 0) {
            var x = buttonGeometry.x + buttonGeometry.width
            d.x = Qt.binding(function() {
                return (x + width < parent.width) ? x
                                                  : Math.max(d.hMargin, parent.width - width)
            })
        } else {
            var rightMargin = parent.width - buttonGeometry.x - buttonGeometry.width
            d.x = Qt.binding(function() {
                var x = parent.width - rightMargin - width
                return (x > d.hMargin) ? x
                                       : d.hMargin
            })
        }
    }

    showAnimation: NumberAnimation {
        target: menu
        property: "opacity"
        from: 0.0
        to: 1.0
        duration: 300
    }

    hideAnimation: ParallelAnimation {
        ScriptAction {
            script: menu.parent.hide()
        }
        NumberAnimation {
            target: menu
            property: "opacity"
            to: 0.0
            duration: 300
        }
    }

    Rectangle {
        anchors.fill: parent
        color: QnTheme.popupMenuBackground
    }
}

