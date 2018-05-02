import QtQuick 2.6

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import "Ptz"
import "Ptz/joystick_utils.js" as JoystickUtils

Item
{
    id: control

    property alias preloaders: preloadersPanel

    readonly property PtzController controller: PtzController
    {
        property bool supportsPresets: capabilities & Ptz.PresetsPtzCapability
        property bool supportsMoveOnTap: capabilities & Ptz.ViewportPtzCapability
    }

    property alias customRotation: joystick.customRotation
    property alias moveOnTapMode: moveOnTapButton.checked

    signal closeButtonClicked()

    function moveViewport(viewportRect, aspect)
    {
        controller.viewportMove(aspect, viewportRect, 1)
    }

    implicitWidth: content.width
    implicitHeight: content.height

    PreloadersPanel
    {
        id: preloadersPanel

        focusPressed: focusControl.focusInPressed || focusControl.focusOutPressed
        zoomInPressed: zoomControl.zoomInPressed
        zoomOutPressed: zoomControl.zoomOutPressed
        moveDirection: joystick.direction
        customRotation: control.customRotation

        Connections
        {
            target: focusControl
            onAutoFocusClicked: preloadersPanel.showCommonPreloader()
        }
        Connections
        {
            target: presetsItem
            onGoToPreset: preloadersPanel.showCommonPreloader()
        }
        Connections
        {
            target: presetsButton
            onPresetChoosen: preloadersPanel.showCommonPreloader()
        }
    }

    Column
    {
        id: content

        x: 4
        width: parent.width - 2 * x

        spacing: 12
        bottomPadding: 4

        Item
        {
            x: 4
            width: parent.width - 2 * x
            height: Math.max(zoomFocusRow.height, joystick.height)

            Row
            {
                id: zoomFocusRow

                spacing: 8
                anchors.bottom: parent.bottom

                FocusControl
                {
                    id: focusControl

                    anchors.bottom: parent.bottom
                    visible: controller.capabilities & Ptz.ContinuousFocusCapability
                    supportsAutoFocus: controller.auxTraits & Ptz.ManualAutoFocusPtzTrait

                    onFocusInPressedChanged: moveFocus(focusInPressed, 1)
                    onFocusOutPressedChanged: moveFocus(focusOutPressed, -1)
                    onAutoFocusClicked: controller.setAutoFocus()

                    function moveFocus(shouldMove, speed)
                    {
                        var focusSpeed = shouldMove? speed : 0
                        controller.continuousFocus(focusSpeed)
                    }
                }

                ZoomControl
                {
                    id: zoomControl

                    anchors.bottom: parent.bottom
                    visible: controller.capabilities & Ptz.ContinuousZoomCapability

                    onZoomInPressedChanged: zoomMove(zoomInPressed, 0.5)
                    onZoomOutPressedChanged: zoomMove(zoomOutPressed, -0.5)

                    function zoomMove(shouldMove, speed)
                    {
                        var zoomVector = shouldMove
                            ? Qt.vector3d(0, 0, speed)
                            : Qt.vector3d(0, 0, 0)

                        controller.continuousMove(zoomVector)
                    }
                }
            }

            Joystick
            {
                id: joystick

                readonly property vector2d zeroVector: Qt.vector2d(0, 0)
                property vector2d movementVector: Qt.vector2d(0, 0)

                joystickType:
                {
                    if (!visible)
                        return JoystickUtils.Type.Any

                    var caps = controller.capabilities
                    if (caps & Ptz.ContinuousPanTiltCapabilities)
                    {
                        if (caps & Ptz.EightWayPtzTrait)
                            return JoystickUtils.Type.EightWayPtz
                        if (caps & Ptz.FourWayPtzTrait)
                            return JoystickUtils.Type.FourWayPtz

                        return JoystickUtils.Type.FreeWayPtz
                    }

                    if (caps & Ptz.ContinuousPanCapability)
                        return JoystickUtils.Type.TwoWayHorizontal

                    if (caps & Ptz.ContinuousTiltCapability)
                        return JoystickUtils.Type.TwoWayVertical

                    return JoystickUtils.Type.Any
                }

                anchors.bottom: parent.bottom
                anchors.right: parent.right
                visible: controller &&
                    (controller.capabilities & Ptz.ContinuousPanCapability
                    || controller.capabilities & Ptz.ContinuousTiltCapability)

                Timer
                {
                    id: directionFilterTimer

                    property vector2d value: joystick.zeroVector

                    interval: 450

                    onTriggered:
                    {
                        if (value != joystick.movementVector)
                            joystick.movementVector = value
                    }
                }

                onMovementVectorChanged:
                {
                    controller.continuousMove(Qt.vector3d(movementVector.x, movementVector.y, 0))
                }

                onDirectionChanged:
                {
                    if (direction == zeroVector)
                        movementVector = directionFilterTimer.value

                    directionFilterTimer.value = direction
                    directionFilterTimer.restart()
                }
            }

            Button
            {
                id: moveOnTapButton

                y: joystick.y - height - 8

                color: ColorTheme.transparent(ColorTheme.base9, 0.8)

                visible: controller.supportsMoveOnTap
                width: 48
                height: width
                radius: width / 2
                checkable: true
                checked: false

                anchors.right: parent.right
                icon: lp("images/ptz/ptz.png")

                padding: 0
                rightPadding: 0
                leftPadding: 0
                topPadding: 0
                bottomPadding: 0
            }
        }

        Item
        {
            width: parent.width
            height: 48

            PresetsButton
            {
                id: presetsButton

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                leftPadding: 0
                rightPadding: 0
                topPadding: 0
                bottomPadding: 0
                padding: 0
                height: parent.height

                resourceId: controller.resourceId
                popupParent: videoScreen
                onPresetChoosen:
                {
                    if (controller.setPresetById(id))
                        presetsItem.currentPresetIndex = controller.indexOfPreset(id)
                }

                visible: controller.presetsCount && controller.supportsPresets
            }

            PresetsListItem
            {
                id: presetsItem

                height: parent.height
                anchors.left: presetsButton.right
                anchors.right: hidePtzButton.left
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter

                presetsCount: controller.presetsCount
                currentPresetIndex: controller.activePresetIndex

                visible: controller.presetsCount && controller.supportsPresets

                Timer
                {
                    id: skipIndexChangedNotificationTimer

                    interval: 10000
                }

                onGoToPreset:
                {
                    presetsItem.currentPresetIndex = index
                    if (index == -1)
                        return

                    controller.setPresetByIndex(index)
                    skipIndexChangedNotificationTimer.restart()
                }

                Connections
                {
                    target: controller
                    onActivePresetIndexChanged:
                    {
                        if (!skipIndexChangedNotificationTimer.running)
                            presetsItem.currentPresetIndex = controller.activePresetIndex
                    }
                }
            }

            Button
            {
                id: hidePtzButton

                width: height
                height: parent.height
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                rightPadding: 0
                leftPadding: 0
                topPadding: 0
                bottomPadding: 0

                flat: true
                icon: lp("/images/close.png")

                onClicked: control.closeButtonClicked()
            }
        }
    }
}
