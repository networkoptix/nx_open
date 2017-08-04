import QtQuick 2.6

import "../VideoScreen/Ptz/joystick_utils.js" as VectorUtils

Item
{
    id: control

    anchors.fill: parent

    property bool zoomInPressed: false
    property bool zoomOutPressed: false
    property bool focusPressed: false
    property vector2d moveDirection

    property bool privateAutoFocusPressed: false
    readonly property vector2d zeroVector: Qt.vector2d(0, 0)

    function showCommonPreloader()
    {
        privateAutoFocusPressed = true; //< Shows autofocus preloader.
        privateAutoFocusPressed = false;
    }

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

        anchors.centerIn: parent
        onItemChanged:
        {
            if (!item)
                return

            item.anchors.centerIn = loader
            item.show()
        }
    }

    function updatePreloaderState()
    {
        var someoneActive = zoomInPressed || zoomOutPressed  || focusPressed
            || privateAutoFocusPressed || moveDirection != zeroVector

        if (!someoneActive)
        {
            if (loader.item)
                loader.item.hide()

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
        if (loader.item)
            loader.item.hide(true)

        loader.sourceComponent = preloader

        if (loader.item)
            loader.item.show()
    }
}
