import QtQuick 2.0
import QtGraphicalEffects 1.0

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
    property bool rippleEffectEnabled: !liteMode
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
        target: mouseArea
        ignoreUnknownSignals: true

        onPressedChanged:
        {
            if (mouseArea.pressed)
            {
                fadeOutAnimation.stop()
                fadeInAnimation.start()
                return
            }

            if (_currentRipple && rippleEffectEnabled)
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

        onPressAndHold:
        {
            if (!rippleEffectEnabled)
                return

            _currentRipple = rippleComponent.createObject(materialEffect)
            _currentRipple.fadeInDuration = 400
            if (centered || mouseArea.mouseX == undefined || mouseArea.mouseY == undefined)
                _currentRipple.fadeIn(width / 2, height / 2)
            else
                _currentRipple.fadeIn(mouseArea.mouseX, mouseArea.mouseY)
        }

        onReleased:
        {
            if (rippleEffectEnabled)
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
                    if (centered || mouseArea.mouseX == undefined || mouseArea.mouseY == undefined)
                        ripple.splash(width / 2, height / 2)
                    else
                        ripple.splash(mouseArea.mouseX, mouseArea.mouseY)
                }
            }

            fadeInAnimation.complete()
            fadeOutAnimation.start()
        }
    }

    Rectangle
    {
        id: highlightRectangle

        anchors.fill: parent
        color: liteMode ? "#18ffffff" : "#30ffffff"
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
