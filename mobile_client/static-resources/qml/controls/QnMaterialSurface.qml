import QtQuick 2.4

MouseArea {
    id: materialSurface

    property color color: "#20ffffff"
    property bool selected: false

    clip: true

    readonly property bool _focused: pressed || selected

    Rectangle {
        id: focusBackground
        anchors.fill: parent
        opacity: materialSurface._focused ? 1.0 : 0.0
        color: materialSurface.color
        Behavior on opacity { OpacityAnimator { duration: 500; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        id: tapCircle

        property real startRadius: parent.width / 6
        property real endRadius

        property real circleX
        property real circleY

        x: circleX - radius
        y: circleY - radius
        width: radius * 2
        height: radius * 2

        color: materialSurface.color
        opacity: 0.0

        Behavior on opacity {
            NumberAnimation {
                id: opacityAnimation
                duration: 300; easing.type: Easing.OutCubic
            }
        }
        Behavior on radius {
            id: radiusBehavior
            enabled: false
            NumberAnimation { duration: 500; easing.type: Easing.OutCubic }
        }

        onOpacityChanged: {
            if (opacity == 1.0) {
                if (!materialSurface._focused)
                    opacity = 0.0
                else
                    opacityAnimation.duration = 500
            }
        }

        function getEndRadius(x, y) {
            var dx = Math.max(x, parent.width - x)
            var dy = Math.max(y, parent.height - y)
            return Math.sqrt(dx * dx + dy * dy)
        }

        function fadeIn(mouseX, mouseY) {
            opacityAnimation.duration = 300

            circleX = mouseX
            circleY = mouseY

            radius = tapCircle.startRadius
            opacity = 1.0

            radiusBehavior.enabled = true

            radius = getEndRadius(mouseX, mouseY)
        }

        function fadeOut() {
            radiusBehavior.enabled = false

            if (tapCircle.opacity > 0.8) {
                opacityAnimation.duration = 500

                if (tapCircle.opacity == 1.0)
                    tapCircle.opacity = 0.0
            }
        }
    }

    Behavior on color { ColorAnimation { duration: opacityAnimation.duration } }

    onSelectedChanged: {
        if (selected) {
            if (!pressed)
                tapCircle.fadeIn(width / 2, height / 2)
        } else {
            if (!pressed)
                tapCircle.fadeOut()
        }
    }

    onPressed: {
        if (!selected)
            tapCircle.fadeIn(mouse.x, mouse.y)
    }
    onReleased: {
        if (!selected)
            tapCircle.fadeOut()
    }
}

