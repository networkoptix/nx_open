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

    property Item privateCurrentPreloader
    property bool privateAutoFocusPressed: false
    readonly property vector2d zeroVector: Qt.vector2d(0, 0)

    function showCommonPreloader()
    {
        privateAutoFocusPressed = true; //< Shows autofocus preloader
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
            movePreloader.rotation = -VectorUtils.getDegrees(moveDirection)
    }

    FocusPreloader
    {
        id: focusPreloader
        anchors.centerIn: parent;
    }

    ContinuousMovePreloader
    {
        id: movePreloader
        anchors.centerIn: parent;
    }

    ContinuousZoomPreloader
    {
        id: zoomInPreloader
        anchors.centerIn: parent;
    }

    ContinuousZoomPreloader
    {
        id: zoomOutPreloader

        zoomInIndicator: false
        anchors.centerIn: parent;
    }

    function updatePreloaderState()
    {
        var someoneActive = zoomInPressed || zoomOutPressed  || focusPressed
            || privateAutoFocusPressed || moveDirection != zeroVector

        if (!someoneActive)
        {
            if (privateCurrentPreloader)
                privateCurrentPreloader.hide()

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
        if (privateCurrentPreloader)
            privateCurrentPreloader.hide(true)

        privateCurrentPreloader = preloader;

        if (privateCurrentPreloader)
            privateCurrentPreloader.show()
    }
}
