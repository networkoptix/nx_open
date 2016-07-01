import QtQuick 2.6
import Qt.labs.controls 1.0

StackView
{
    id: stackView

    property int transitionDuration: 200

    QtObject
    {
        id: d

        property real scaleInXHint: -1
        property real scaleInYHint: -1
        property real maxShift: 80

        function transitionFinished(properties)
        {
            properties.exitItem.x = 0.0
            properties.exitItem.scale = 1.0
            properties.exitItem.opacity = 1.0
            scaleInXHint = -1
            scaleInYHint = -1
        }

        function getFromX()
        {
            if (scaleInXHint < 0)
                return 0

            var dx = scaleInXHint - stackView.width / 2
            var normalized = Math.max(Math.abs(dx), maxShift)
            return dx > 0 ? normalized : -normalized
        }

        function getFromY()
        {
            if (scaleInYHint < 0)
                return 0

            var dy = scaleInYHint - stackView.height / 2
            var normalized = Math.max(Math.abs(dy), maxShift)
            return dy > 0 ? normalized : -normalized
        }
    }

    replaceEnter: Transition
    {
        id: fadeInTransition

        SequentialAnimation
        {
            NumberAnimation
            {
                property: "opacity"
                to: 0
                duration: 0
            }
            PauseAnimation
            {
                duration: stackView.transitionDuration
            }
            NumberAnimation
            {
                property: "opacity"
                from: 0
                to: 1
                duration: stackView.transitionDuration
            }
        }
    }

    replaceExit: Transition
    {
        id: fadeOutTransition

        NumberAnimation
        {
            property: "opacity"
            from: 1
            to: 0
            duration: stackView.transitionDuration
        }
    }

    pushEnter: Transition
    {
        id: scaleInTransition

        NumberAnimation
        {
            property: "opacity"
            from: 0
            to: 1
            duration: stackView.transitionDuration
        }
        NumberAnimation
        {
            property: "scale"
            from: 0.8
            to: 1.0
            duration: stackView.transitionDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation
        {
            property: "x"
            from: d.getFromX()
            to: 0
            duration: stackView.transitionDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation
        {
            property: "y"
            from: d.getFromY()
            to: 0
            duration: stackView.transitionDuration
            easing.type: Easing.OutCubic
        }
    }

    pushExit: null

    popEnter: Transition
    {
        id: justShowTransition

        NumberAnimation
        {
            property: "opacity"
            to: 1
            duration: 0
        }
        NumberAnimation
        {
            property: "x"
            to: 0
            duration: 0
        }
    }

    popExit: Transition
    {
        id: scaleOutTransition

        NumberAnimation
        {
            property: "opacity"
            from: 1
            to: 0
            duration: stackView.transitionDuration
        }
        NumberAnimation
        {
            property: "scale"
            from: 1.0
            to: 0.8
            duration: stackView.transitionDuration
            easing.type: Easing.OutCubic
        }
    }

    function setScaleTransitionHint(xHint, yHint)
    {
        d.scaleInXHint = (xHint == undefined) ? -1 : xHint
        d.scaleInYHint = (yHint == undefined) ? -1 : yHint
    }
}
