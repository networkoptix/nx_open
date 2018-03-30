import QtQuick 2.6

ListView
{
    id: control

    property int pressedStateFilterMs: 400
    property int emptyHeaderSize: 4
    readonly property alias scrollable: d.prefferToBeInteractive

    signal initiallyPressed(int index)
    signal buttonClicked(int index)
    signal pressedChanged(int index, bool pressed)
    signal actionCancelled(int index)
    signal enabledChanged(int index, bool buttonEnabled)

    pressDelay: 150
    clip: true
    layoutDirection: Qt.RightToLeft
    orientation: Qt.Horizontal
    implicitHeight: 56

    interactive: d.allowInteractiveState && scrollable

    leftMargin: emptyHeaderSize
    rightMargin: emptyHeaderSize

    function forceInitialSlideAnimation() { delayedAnimationTimer.restart() }

    onVisibleChanged:
    {
        if (visible && interactive)
            forceInitialSlideAnimation()
    }

    Timer
    {
        id: delayedAnimationTimer

        interval: 30
        onTriggered: showAnimation.restart()
    }

    PropertyAnimation
    {
        id: showAnimation

        duration: 300
        easing.type: Easing.OutQuad
        target: control
        properties: "contentX"
        from: -width - height + emptyHeaderSize
        to: -width + emptyHeaderSize
    }

    Image
    {
        source: "qrc:///images/bottom_panel_shadow_left.png"
        visible: d.prefferToBeInteractive && visibleArea.xPosition > 0
    }

    Image
    {
        source: "qrc:///images/bottom_panel_shadow_right.png"
        anchors.right: parent.right
        visible: d.prefferToBeInteractive && (visibleArea.widthRatio + visibleArea.xPosition < 1)
    }

    delegate: IconButton
    {
        id: button

        property bool buttonPressed: false
        property bool active: false

        icon: model.iconPath
        enabled: model.enabled
        padding: 0
        anchors.verticalCenter: parent.verticalCenter

        onButtonPressedChanged: control.pressedChanged(index, buttonPressed)
        onEnabledChanged: control.enabledChanged(index, enabled)

        Connections
        {
            target: control

            onFlickingChanged: handleCancelled()
            onDraggingChanged: handleCancelled()
        }

        property bool filteringPressing: false

        onClicked:
        {
            if (!filteringPressing)
            {
                pressedStateFilterTimer.stop()
                control.buttonClicked(index)
            }
        }

        onPressed:
        {
            button.active = true
            control.initiallyPressed(index)
            pressedStateFilterTimer.restart()
        }

        onReleased:
        {
            handleButtonReleased()
            button.active = false
        }

        onCanceled:
        {
            if (!buttonPressed)
                handleCancelled()
            else
                handleButtonReleased()

            button.active = false
        }

        Timer
        {
            id: pressedStateFilterTimer
            interval: control.pressedStateFilterMs
            onTriggered: finishStateProcessing(true)
        }

        function handleButtonReleased()
        {
            if (pressedStateFilterTimer.running)
                pressedStateFilterTimer.stop()
            else
                finishStateProcessing(false)
        }

        function finishStateProcessing(value)
        {
            if (!button.active)
                return

            button.filteringPressing = value
            button.buttonPressed = value
            d.allowInteractiveState = !value

            if (!value)
                button.active = false
        }

        function handleCancelled()
        {
            if (pressedStateFilterTimer.running)
                pressedStateFilterTimer.stop()
            else
                buttonPressed = false

            if (button.active)
                control.actionCancelled(index)

            button.active = false
        }
    }

    QtObject
    {
        id: d

        property bool allowInteractiveState: true
        property bool prefferToBeInteractive: contentWidth > width
    }
}
