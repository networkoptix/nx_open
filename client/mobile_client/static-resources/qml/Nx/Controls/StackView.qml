import QtQuick 2.6
import Qt.labs.controls 1.0

StackView
{
    id: stackView

    property int transitionDuration: 200

    function safePush(url, properties, operation)
    {
        if (busy)
            return undefined

        var component = Qt.createComponent(url)
        var item = properties
            ? component.createObject(null, properties)
            : component.createObject(null)

        var error = component.errorString()
        if (error.length)
            console.log("Error whilte creating component:", error)

        if (currentItem && currentItem.objectName == item.objectName)
        {
            item.destroy()
            return undefined
        }

        return push(item, properties, operation)
    }

    function safeReplace(target, item, properties, operation)
    {
        return busy
            ? undefined
            : replace(target, item, properties, operation)
    }

    onBusyChanged:
    {
        if (!busy && currentItem)
            currentItem.forceActiveFocus()
    }

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
            PropertyAction
            {
                property: "opacity"
                value: 0
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

        PropertyAction
        {
            property: "opacity"
            value: 1
        }
        PropertyAction
        {
            property: "x"
            value: 0
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
