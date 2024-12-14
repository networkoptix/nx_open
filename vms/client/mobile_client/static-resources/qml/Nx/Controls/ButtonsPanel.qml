// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core

ListView
{
    id: control

    readonly property bool denyFlickableVisibleAreaCorrection: true

    property int pressedStateFilterMs: 500
    property int emptyHeaderSize: 4
    readonly property alias scrollable: d.prefferToBeInteractive
    property bool blockMouseEvents: false

    signal buttonDownChanged(int index, bool down)
    signal buttonPressed(int index)
    signal buttonClicked(int index)
    signal longPressedChanged(int index, bool pressed, bool down)
    signal actionCancelled(int index)
    signal buttonEnabledChanged(int index, bool buttonEnabled)

    pressDelay: 100

    clip: true
    layoutDirection: Qt.RightToLeft
    orientation: Qt.Horizontal
    implicitHeight: 56

    interactive: scrollable

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

        readonly property bool instantAction: model.type === CameraButton.Type.instant
        property bool buttonLongPressed: false
        property bool active: false
        property bool filteringPressing: false

        icon.source: model.iconPath
        enabledMask: model.enabled
        padding: 0
        anchors.verticalCenter: parent ? parent.verticalCenter : undefined

        onButtonLongPressedChanged:
        {
            if (active)
                control.longPressedChanged(index, buttonLongPressed, button.down)
        }

        onDownChanged: pressedSignalOrderTimer.restart()
        onEnabledMaskChanged: control.buttonEnabledChanged(index, enabledMask)

        Connections
        {
            target: control

            function onFlickingChanged()
            {
                handleMovementChanged()
            }

            function onDraggingChanged()
            {
                handleMovementChanged()
            }

            function handleMovementChanged()
            {
                if (button.instantAction)
                    button.handleCancelled()

                var finished = !control.flicking && !control.dragging
                if (finished && button.active)
                    button.handleButtonReleased()
            }
        }

        onPressed:
        {
            filteringPressing = false
            buttonLongPressed = false

            button.active = true
            control.buttonPressed(index)
            if (!instantAction/*model.disableLongPress*/)
                pressedStateFilterTimer.restart()
        }

        onReleased:
        {
            handleButtonReleased()
            button.active = false
        }

        onCanceled:
        {
            if (control.dragging || control.flicking)
                return

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
                    control.buttonDownChanged(index, button.down)
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
            if (pressedStateFilterTimer.running || instantAction/*model.disableLongPress*/)
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

        Component.onDestruction: handleCancelled()
    }

    NxObject
    {
        id: d

        property real offset: control.originX + control.contentWidth
        property bool prefferToBeInteractive: contentWidth > width
    }
}
