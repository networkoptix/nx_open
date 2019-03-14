import QtQuick 2.6

import "Ptz/joystick_utils.js" as VectorUtils

Item
{
    id: control

    property bool zoomInPressed: false
    property bool zoomOutPressed: false
    property bool focusPressed: false
    property vector2d moveDirection

    property bool privateAutoFocusPressed: false
    readonly property vector2d zeroVector: Qt.vector2d(0, 0)

    property real customRotation;

    function showCommonPreloader()
    {
        privateAutoFocusPressed = true; //< Shows autofocus preloader.
        privateAutoFocusPressed = false;
    }

    implicitWidth: loader.width
    implicitHeight: loader.height

    onZoomInPressedChanged: updatePreloaderState()
    onZoomOutPressedChanged: updatePreloaderState()
    onFocusPressedChanged: updatePreloaderState()
    onPrivateAutoFocusPressedChanged: updatePreloaderState()
    onMoveDirectionChanged:
    {
        updatePreloaderState()
        if (moveDirection != zeroVector)
            loader.item.rotation = -VectorUtils.getDegrees(moveDirection)
    }

    Component
    {
        id: focusPreloader
        FocusPreloader {}
    }

    Component
    {
        id: movePreloader
        ContinuousMovePreloader {}
    }

    Component
    {
        id: zoomInPreloader
        ContinuousZoomPreloader {}
    }

    Component
    {
        id: zoomOutPreloader

        ContinuousZoomPreloader
        {
            zoomInIndicator: false
        }
    }

    Loader
    {
        id: loader

        rotation: sourceComponent == movePreloader ? control.customRotation : 0
        anchors.centerIn: parent
        onItemChanged:
        {
            if (item)
                item.show()
        }
    }

    function updatePreloaderState()
    {
        var someoneActive = zoomInPressed || zoomOutPressed  || focusPressed
            || privateAutoFocusPressed || moveDirection != zeroVector

        if (!someoneActive)
        {
            setCurrentPreloader(undefined)
            return
        }

        if (zoomInPressed)
            setCurrentPreloader(zoomInPreloader)
        else if (zoomOutPressed)
            setCurrentPreloader(zoomOutPreloader)
        else if (focusPressed || privateAutoFocusPressed)
            setCurrentPreloader(focusPreloader)
        else if (moveDirection != zeroVector)
            setCurrentPreloader(movePreloader)
    }

    function setCurrentPreloader(preloader)
    {
        var emptyPreloader = !preloader
        var immediately = !emptyPreloader
        if (loader.item)
            loader.item.hide(immediately)

        if (emptyPreloader)
            return

        loader.sourceComponent = preloader
        loader.item.show()
        loader.item.visibleChanged.connect(
            function()
            {
                if (loader && loader.item && !loader.item.visible)
                    loader.sourceComponent = undefined
            })
    }
}
