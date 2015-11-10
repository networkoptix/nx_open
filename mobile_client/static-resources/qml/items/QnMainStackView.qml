import QtQuick 2.4
import QtQuick.Controls 1.2

StackView {
    id: stackView

    function setSlideTransition() {
        stackViewDelegate.pushTransition = slideInTransition
        stackViewDelegate.popTransition = slideOutTransition
    }

    function setInvertedSlideTransition() {
        stackViewDelegate.pushTransition = slideOutTransition
        stackViewDelegate.popTransition = slideInTransition
    }

    function setFadeTransition() {
        stackViewDelegate.pushTransition = fadeTransition
        stackViewDelegate.popTransition = fadeTransition
    }

    function setScaleTransition(xHint, yHint) {
        stackViewDelegate.scaleInXHint = xHint == undefined ? -1 : xHint
        stackViewDelegate.scaleInYHint = yHint == undefined ? -1 : yHint

        stackViewDelegate.pushTransition = scaleInTransition
        stackViewDelegate.popTransition = scaleOutTransition
    }

    delegate: StackViewDelegate {
        id: stackViewDelegate

        property real scaleInXHint: -1
        property real scaleInYHint: -1
        readonly property real maxShift: dp(80)

        pushTransition: slideInTransition
        replaceTransition: slideInTransition
        popTransition: slideOutTransition

        function transitionFinished(properties) {
            properties.exitItem.x = 0.0
            properties.exitItem.scale = 1.0
            properties.exitItem.opacity = 1.0
            scaleInXHint = -1
            scaleInYHint = -1
        }

        function getFromX() {
            if (scaleInXHint < 0)
                return 0

            var dx = scaleInXHint - stackView.width / 2
            var normalized = Math.max(Math.abs(dx), maxShift)
            return dx > 0 ? normalized : -normalized
        }

        function getFromY() {
            if (scaleInYHint < 0)
                return 0

            var dy = scaleInYHint - stackView.height / 2
            var normalized = Math.max(Math.abs(dy), maxShift)
            return dy > 0 ? normalized : -normalized
        }

    }

    Component {
        id: slideInTransition

        StackViewTransition {
            id: transition

            readonly property int transitionDuration: 200

            PropertyAnimation {
                target: enterItem
                property: "x"
                from: enterItem.width
                to: 0
                duration: transition.transitionDuration
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                target: exitItem
                property: "x"
                from: 0
                to: -exitItem.width
                duration: transition.transitionDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Component {
        id: slideOutTransition

        StackViewTransition {
            id: transition

            readonly property int transitionDuration: 200

            PropertyAnimation {
                target: enterItem
                property: "x"
                from: -enterItem.width
                to: 0
                duration: transition.transitionDuration
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                target: exitItem
                property: "x"
                from: 0
                to: exitItem.width
                duration: transition.transitionDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Component {
        id: scaleInTransition

        StackViewTransition {
            id: transition

            readonly property int transitionDuration: 200

            PropertyAnimation {
                target: enterItem
                property: "opacity"
                from: 0.0
                to: 1.0
                duration: transition.transitionDuration
            }
            PropertyAnimation {
                target: enterItem
                property: "scale"
                from: 0.8
                to: 1.0
                duration: transition.transitionDuration
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                target: enterItem
                property: "x"
                from: stackViewDelegate.getFromX()
                to: 0
                duration: transition.transitionDuration
                easing.type: Easing.OutCubic
            }
            PropertyAnimation {
                target: enterItem
                property: "y"
                from: stackViewDelegate.getFromY()
                to: 0
                duration: transition.transitionDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Component {
        id: scaleOutTransition

        StackViewTransition {
            id: transition

            readonly property int transitionDuration: 200

            PropertyAnimation {
                target: exitItem
                property: "opacity"
                from: 1.0
                to: 0.0
                duration: transition.transitionDuration
            }
            PropertyAnimation {
                target: exitItem
                property: "scale"
                from: 1.0
                to: 0.8
                duration: transition.transitionDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Component {
        id: fadeTransition

        StackViewTransition {
            id: transition

            readonly property int transitionDuration: 200

            SequentialAnimation {
                ScriptAction {
                    script: enterItem.opacity = 0.0
                }
                PropertyAnimation {
                    target: exitItem
                    property: "opacity"
                    from: 1.0
                    to: 0.0
                    duration: transition.transitionDuration
                }
                PropertyAnimation {
                    target: enterItem
                    property: "opacity"
                    from: 0.0
                    to: 1.0
                    duration: transition.transitionDuration
                }
            }
        }
    }

}
