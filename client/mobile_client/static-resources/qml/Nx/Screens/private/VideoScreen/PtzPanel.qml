import QtQuick 2.6

import Nx.Core 1.0
import Nx.Controls 1.0

import "Ptz"
import "Ptz/joystick_utils.js" as JoystickUtils

Column
{
    id: ptzPanel

    property alias uniqueResourceId: ptzController.uniqueResourceId

    visible: ptzController.available
    spacing: 8

    PtzController
    {
        id: ptzController

        property bool supportsPresets:
            ptzController.capabilities & Ptz.PresetsPtzCapability
    }

    Item
    {
        id: topPtzPanel

        width: parent.width
        height: Math.max(zoomFocusRow.height, joystick.height)

        Row
        {
            id: zoomFocusRow

            spacing: 8

            FocusControl
            {
                anchors.bottom: parent.bottom
                visible: ptzController.capabilities & Ptz.ContinuousFocusCapability
                supportsAutoFocus: ptzController.auxTraits & Ptz.ManualAutoFocusPtzTrait

                onFocusInPressedChanged: moveFocus(focusInPressed, 1)
                onFocusOutPressedChanged: moveFocus(focusOutPressed, -1)
                onAutoFocusClicked: ptzController.setAutoFocus()

                function moveFocus(shouldMove, speed)
                {
                    var focusSpeed = shouldMove? speed : 0
                    ptzController.continuousFocus(focusSpeed)
                }
            }

            ZoomControl
            {
                anchors.bottom: parent.bottom
                visible: ptzController.capabilities & Ptz.ContinuousZoomCapability

                onZoomInPressedChanged: zoomMove(zoomInPressed, 0.5)
                onZoomOutPressedChanged: zoomMove(zoomOutPressed, -0.5)

                function zoomMove(shouldMove, speed)
                {
                    var zoomVector = shouldMove
                        ? Qt.vector3d(0, 0, speed)
                        : Qt.vector3d(0, 0, 0)

                    ptzController.continuousMove(zoomVector)
                }
            }
        }

        Joystick
        {
            id: joystick

            joystickType:
            {
                if (!visible)
                    return JoystickUtils.Type.Any

                var caps = ptzController.capabilities
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
            visible: ptzController &&
                (ptzController.capabilities & Ptz.ContinuousPanCapability
                || ptzController.capabilities & Ptz.ContinuousTiltCapability)

            onDirectionChanged:
            {
                ptzController.continuousMove(Qt.vector3d(direction.x, direction.y, 0))
            }
        }
    }

    Item
    {
        width: parent.width
        height: Math.max(presetsItem.height)

        Row
        {
            anchors.left: parent.left
            anchors.right: hidePtzButton.left
            visible: ptzController.presetsCount && ptzController.supportsPresets

            PresetsButton
            {
                uniqueResourceId: ptzController.uniqueResourceId
                popupParent: videoScreen

                onPresetChoosen: ptzController.setPresetById(id)
            }

            PresetsListItem
            {
                id: presetsItem

                presetsCount: ptzController.presetsCount
                currentPresetIndex: ptzController.activePresetIndex

                visible: ptzController.presetsCount && ptzController.supportsPresets

                onGoToPreset:
                {
                    if (presetIndex != -1)
                        ptzController.setPresetByIndex(presetIndex)
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
        }
    }
}
