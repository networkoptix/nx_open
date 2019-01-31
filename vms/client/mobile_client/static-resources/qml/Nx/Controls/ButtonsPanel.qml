import QtQuick 2.6

ListView
{
    id: control

    property int pressedStateFilterMs: 500
    property int emptyHeaderSize: 4
    readonly property alias scrollable: d.prefferToBeInteractive
    property bool blockMouseEvents: false

    signal buttonDownChanged(int index, bool down)
    signal buttonPressed(int index)
    signal buttonClicked(int index)
    signal longPressedChanged(int index, bool pressed, bool down)
    signal actionCancelled(int index)
    signal enabledChanged(int index, bool buttonEnabled)

    pressDelay: 100

    clip: true
    layoutDirection: Qt.RightToLeft
    orientation: Qt.Horizontal
    implicitHeight: 56

    interactive: d.allowInteractiveState && scrollable

    leftMargin: emptyHeaderSize
    rightMargin: emptyHeaderSize

    function forceInitialSlideAnimation()
    {
        if (!interactive)
            return

        delayedAnimationTimer.restart()
        contentX = -width - height + emptyHeaderSize + d.offset
    }

    Timer
    {
        id: delayedAnimationTimer

        interval: 200
        onTriggered: showAnimation.restart()

    }

    PropertyAnimation
    {
        id: showAnimation

        duration: 300
        easing.type: Easing.InOutQuad
        target: control
        properties: "contentX"
        to: -width + emptyHeaderSize + d.offset
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

    MouseArea
    {
        anchors.fill: parent
        enabled: control.blockMouseEvents || showAnimation.running
        z: 1
    }

    delegate: IconButton
    {
        id: button

        readonly property bool instantAction: !model.prolongedAction
        property bool buttonLongPressed: false
        property bool active: false
        property bool filteringPressing: false

        icon.source: model.iconPath
        enabled: model.enabled
        padding: 0
        anchors.verticalCenter: parent.verticalCenter

        onButtonLongPressedChanged:
        {
            if (active)
                control.longPressedChanged(index, buttonLongPressed, button.pressed)
        }

        onPressedChanged: pressedSignalOrderTimer.restart()
        onEnabledChanged: control.enabledChanged(index, enabled)

        Connections
        {
            target: control

            onFlickingChanged: handleCancelled()
            onDraggingChanged: handleCancelled()
        }

        onPressed:
        {
            filteringPressing = false
            buttonLongPressed = false
            d.allowInteractiveState = false

            button.active = true
            control.buttonPressed(index)
            if (!model.disableLongPress)
                pressedStateFilterTimer.restart()
        }

        onReleased:
        {
            d.allowInteractiveState = true

            handleButtonReleased()
            button.active = false
        }

        onCanceled:
        {
            d.allowInteractiveState = true

            if (!buttonLongPressed || instantAction)
                handleCancelled()
            else
                handleButtonReleased()

            button.active = false
        }

        Timer
        {
            id: pressedSignalOrderTimer

            interval: 0
            onTriggered:
            {
                if (button.active)
                    control.buttonDownChanged(index, button.pressed)
            }
        }

        Timer
        {
            id: pressedStateFilterTimer
            interval: control.pressedStateFilterMs
            onTriggered: finishStateProcessing(true)
        }

        function handleButtonReleased()
        {
            if (pressedStateFilterTimer.running || model.disableLongPress)
            {
                pressedStateFilterTimer.stop()
                control.buttonClicked(index)
            }
            else
            {
                finishStateProcessing(false)
            }
        }

        function finishStateProcessing(value)
        {
            if (!button.active)
                return

            button.filteringPressing = value
            button.buttonLongPressed = value


            if (!value)
                button.active = false
        }

        function handleCancelled()
        {
            var wasActive = button.active
            button.active = false
            if (pressedStateFilterTimer.running)
                pressedStateFilterTimer.stop()
            else
                buttonLongPressed = false

            if (wasActive)
                control.actionCancelled(index)
        }
    }

    QtObject
    {
        id: d

        property real offset: control.originX + control.contentWidth
        property bool allowInteractiveState: true
        property bool prefferToBeInteractive: contentWidth > width
    }
}
