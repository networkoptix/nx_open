// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import Qt5Compat.GraphicalEffects

Item
{
    id: materialEffect

    property alias highlightColor: highlightRectangle.color
    property color rippleColor: highlightColor
    property real rippleSize: 48
    property var mouseArea: null
    property bool centered: false
    property bool rounded: false
    property real radius: rounded ? width / 2 : 0
    property bool alwaysCompleteHighlightAnimation: false
    property Ripple _currentRipple: null

    layer.enabled: radius > 0
    layer.effect: OpacityMask
    {
        maskSource: Rectangle
        {
            width: materialEffect.width
            height: materialEffect.height
            radius: materialEffect.radius
        }
    }

    Connections
    {
        readonly property var mouseX: mouseArea.mouseX ?? mouseArea.pressX
        readonly property var mouseY: mouseArea.mouseY ?? mouseArea.pressY

        target: mouseArea
        ignoreUnknownSignals: true

        function onPressedChanged()
        {
            if (mouseArea.pressed)
            {
                fadeOutAnimation.stop()
                fadeInAnimation.start()
                return
            }

            if (_currentRipple)
            {
                fadeOutAnimation.duration = _currentRipple.fadeOutDuration
                _currentRipple.fadeOut()
                _currentRipple = null
            }
            else
            {
                fadeOutAnimation.duration = 500
            }

            if (alwaysCompleteHighlightAnimation)
                fadeInAnimation.complete()
            else
                fadeInAnimation.stop()
            fadeOutAnimation.start()
        }

        function onPressAndHold()
        {
            _currentRipple = rippleComponent.createObject(materialEffect)
            _currentRipple.fadeInDuration = 400

            if (centered || mouseX == undefined || mouseY == undefined)
                _currentRipple.fadeIn(width / 2, height / 2)
            else
                _currentRipple.fadeIn(mouseX, mouseY)
        }

        function releaseAt(x, y)
        {
            if (_currentRipple)
            {
                fadeOutAnimation.duration = _currentRipple.fadeOutDuration
                _currentRipple.fadeOut()
                _currentRipple = null
            }
            else
            {
                var ripple = rippleComponent.createObject(materialEffect)
                fadeOutAnimation.duration = ripple.scaleDuration
                if (centered || x === undefined || y === undefined)
                    ripple.splash(width / 2, height / 2)
                else
                    ripple.splash(x, y)
            }

            fadeInAnimation.complete()
            fadeOutAnimation.start()
        }

        function onReleased() { releaseAt(mouseX, mouseY) }

        function onTapped(eventPoint, button)
        {
            releaseAt(eventPoint.position.x, eventPoint.position.y)
        }
    }

    Rectangle
    {
        id: highlightRectangle

        anchors.fill: parent
        color: "#33ffffff"
        opacity: 0

        SequentialAnimation
        {
            id: fadeInAnimation

            PauseAnimation
            {
                duration: 100
            }
            NumberAnimation
            {
                target: highlightRectangle
                property: "opacity"
                to: 1.0
                duration: 100
                easing.type: Easing.InOutQuad
            }
        }

        NumberAnimation on opacity
        {
            id: fadeOutAnimation

            running: false
            to: 0.0
            duration: 500
            easing.type: Easing.InOutQuad
        }
    }

    Component
    {
        id: rippleComponent

        Ripple
        {
            size: rippleSize
            color: rippleColor
        }
    }
}
