import QtQuick 2.6

ListView
{
    id: control

    property int pressedStateFilterMs: 500
    property int emptyHeaderSize: 4
    readonly property alias scrollable: d.prefferToBeInteractive

    signal buttonClicked(int index)
    signal pressedChanged(int index, bool pressed)

    clip: true
    layoutDirection: Qt.RightToLeft
    orientation: Qt.Horizontal
    implicitHeight: 56

    interactive: d.allowInteractiveState && scrollable

    function forceAnimation() { showAnimation.restart() }

    onVisibleChanged:
    {
        if (visible && interactive)
            forceAnimation()
    }

    SequentialAnimation
    {
        id: showAnimation

        ScriptAction { script: contentX = -width + emptyHeaderSize}

        PropertyAnimation
        {
            duration: 300
            easing.type: Easing.OutQuad
            target: control
            properties: "contentX"
            from: -width - height + emptyHeaderSize
            to: -width + emptyHeaderSize
        }
    }

    header: Item
    {
        width: emptyHeaderSize
        height: control.height
        visible: emptyHeaderSize > 0
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

        icon: model.iconPath
        padding: 0

        anchors.verticalCenter: parent.verticalCenter

        property bool buttonPressed: false

        onButtonPressedChanged: control.pressedChanged(index, buttonPressed)

        Connections
        {
            target: control

            onFlickingChanged: pressedStateFilterTimer.stop();
            onDraggingChanged: pressedStateFilterTimer.stop();
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
            if (model.allowLongPress)
                pressedStateFilterTimer.restart()
            else
                pressedStateFilterTimer.stop()
        }

        onReleased: handleButtonReleased()
        onCanceled: handleButtonReleased()

        onPressedChanged:
        {   
            if (!buttonPressed && pressedStateFilterTimer.running)
                pressedStateFilterTimer.stop()
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
            button.filteringPressing = value
            button.buttonPressed = value
            d.allowInteractiveState = !value
        }
    }

    QtObject
    {
        id: d

        property bool allowInteractiveState: true
        property bool prefferToBeInteractive: contentWidth > width
    }
}
