import QtQuick 2.6

import Nx.Core 1.0
import Nx.Controls 1.0

import "Ptz"
import "Ptz/joystick_utils.js" as JoystickUtils

Item
{
    id: control

    property PtzController controller: PtzController
    {
        property bool supportsPresets: capabilities & Ptz.PresetsPtzCapability
    }

    property alias customRotation: joystick.customRotation

    signal closeButtonClicked()

    implicitWidth: content.width
    implicitHeight: content.height

    Image
    {
        width: parent.width
        height: sourceSize.height
        anchors.bottom: parent.bottom
        sourceSize.height: 56 * 2
        source: lp("/images/timeline_gradient.png")
    }

    Column
    {
        id: content

        x: 8
        width: parent.width - 16

        spacing: 8

        Item
        {
            id: topPtzPanel

            width: parent.width
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
        }

        Item
        {
            width: parent.width
            height: 56

            PresetsButton
            {
                id: presetsButton

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter

                resourceId: controller.resourceId
                popupParent: videoScreen
                onPresetChoosen: controller.setPresetById(id)

                visible: controller.presetsCount && controller.supportsPresets
            }

            PresetsListItem
            {
                id: presetsItem

                anchors.left: presetsButton.right
                anchors.right: hidePtzButton.left
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter

                presetsCount: controller.presetsCount
                currentPresetIndex: controller.activePresetIndex

                visible: controller.presetsCount && controller.supportsPresets

                onCurrentPresetIndexChanged:
                {
                    if (currentPresetIndex != -1)
                        controller.setPresetByIndex(currentPresetIndex)
                }

                Connections
                {
                    target: controller
                    onActivePresetIndexChanged:
                    {
                        presetsItem.currentPresetIndex = controller.activePresetIndex
                    }
                }
            }

            Button
            {
                id: hidePtzButton

                width: 48
                height: width
                anchors.right: parent.right
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter

                flat: true
                icon: lp("/images/close.png")

                onClicked: control.closeButtonClicked()
            }
        }
    }
}
