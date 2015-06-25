import QtQuick 2.4

Item {
    id: materialSurface

    property color color: "#20ffffff"
    property bool selected: false
    property bool centered: false
    property bool circular: false
    property bool backlight: true
    property real explicitRadius: 0

    readonly property alias pressed: mouseArea.pressed
    property alias drag: mouseArea.drag

    signal clicked(real x, real y)
    signal pressAndHold(real x, real y)

    clip: true
    anchors.fill: parent

    readonly property bool _focused: mouseArea.pressed || selected

    function _length(dx, dy) {
        return Math.sqrt(dx * dx + dy * dy) - dp(4)
    }

    Rectangle {
        id: focusBackground
        width: explicitRadius > 0 ? explicitRadius * 2 :
                                    circular ? _length(parent.width, parent.height) : parent.width
        height: explicitRadius > 0 ? explicitRadius * 2 :
                                     circular ? _length(parent.width, parent.height) : parent.height
        anchors.centerIn: parent
        visible: backlight
        opacity: 0.0
        color: materialSurface.color
        radius: materialSurface.circular ? Math.min(width, height) / 2 : 0
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
                if (!materialSurface._focused) {
                    opacity = 0.0
                    focusBackground.opacity = 0.0
                } else {
                    opacityAnimation.duration = 500
                }
            }
        }

        function getEndRadius(x, y) {
            if (explicitRadius > 0)
                return explicitRadius

            return _length(Math.max(x, parent.width - x), Math.max(y, parent.height - y))
        }

        function fadeIn(mouseX, mouseY) {
            opacityAnimation.duration = 300

            if (materialSurface.centered) {
                circleX = materialSurface.width / 2
                circleY = materialSurface.height / 2
            } else {
                circleX = mouseX
                circleY = mouseY
            }

            radius = tapCircle.startRadius
            opacity = 1.0
            focusBackground.opacity = 1.0

            radiusBehavior.enabled = true

            radius = getEndRadius(circleX, circleY)
        }

        function fadeOut() {
            radiusBehavior.enabled = false

            if (tapCircle.opacity > 0.8) {
                opacityAnimation.duration = 500

                if (tapCircle.opacity == 1.0) {
                    tapCircle.opacity = 0.0
                    focusBackground.opacity = 0.0
                }
            }
        }
    }

    Behavior on color { ColorAnimation { duration: opacityAnimation.duration } }

    onSelectedChanged: {
        if (selected) {
            if (!mouseArea.pressed)
                tapCircle.fadeIn(width / 2, height / 2)
        } else {
            if (!mouseArea.pressed)
                tapCircle.fadeOut()
        }
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent

        property real pressX
        property real pressY

        onPressed: {
            pressX = mouse.x
            pressY = mouse.y
            fadeInTimer.restart()
        }
        onReleased: {
            if (!selected) {
                if (fadeInTimer.running)
                    tapCircle.fadeIn(pressX, pressY)
                else
                    tapCircle.fadeOut()
            }
            fadeInTimer.stop()
            if (!drag.active)
                clickDelayTimer.start()
        }
        onPressedChanged: {
            if (!pressed) {
                if (!fadeInTimer.running && !selected)
                    tapCircle.fadeOut()
                fadeInTimer.stop()
            }
        }

        drag.onActiveChanged: {
            if (!drag.active)
                return

            if (!fadeInTimer.running && !selected)
                tapCircle.fadeOut()
            fadeInTimer.stop()
        }

        onPressAndHold: materialSurface.pressAndHold(mouse.x, mouse.y)

        Timer {
            id: fadeInTimer

            repeat: false
            running: false
            interval: 100

            onTriggered: {
                if (!selected)
                    tapCircle.fadeIn(mouseArea.pressX, mouseArea.pressY)
            }
        }

        Timer {
            id: clickDelayTimer

            repeat: false
            running: false
            interval: opacityAnimation.duration

            onTriggered: materialSurface.clicked(mouseArea.pressX, mouseArea.pressY)
        }
    }
}

