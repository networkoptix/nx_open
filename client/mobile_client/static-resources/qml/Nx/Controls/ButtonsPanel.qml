import QtQuick 2.6

ListView
{
    id: control

    property int pressedStateFilterMs: 500

    signal buttonClicked(int index)
    signal pressedChanged(int index, bool pressed)

    clip: true
    layoutDirection: Qt.RightToLeft
    orientation: Qt.Horizontal
    implicitHeight: 48

    interactive: d.allowInteractiveState && contentWidth > width

    function forceAnimation() { showAnimation.restart() }

    onVisibleChanged:
    {
        if (visible && interactive)
            forceAnimation()
    }

    SequentialAnimation
    {
        id: showAnimation

        ScriptAction { script: contentX = -width }

        PropertyAnimation
        {
            duration: 300
            easing.type: Easing.OutQuad
            target: control
            properties: "contentX"
            from: -width - height
            to: -width
        }
    }

    Image
    {
        source: "qrc:///images/bottom_panel_shadow_left.png"
        anchors.left: parent.left
        visible: interactive && visibleArea.xPosition > 0
    }

    Image
    {
        source: "qrc:///images/bottom_panel_shadow_right.png"
        anchors.right: parent.right
        visible: interactive && (visibleArea.widthRatio + visibleArea.xPosition < 1)
    }

    delegate: IconButton
    {
        id: button

        icon: model.iconPath
        padding: 0

        anchors.verticalCenter: parent.verticalCenter

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

        onPressed: pressedStateFilterTimer.restart()
        onReleased: handleButtonReleased()
        onCanceled: handleButtonReleased()

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
            control.pressedChanged(index, value)
            d.allowInteractiveState = !value
        }
    }

    QtObject
    {
        id: d

        property bool allowInteractiveState: true
    }
}
