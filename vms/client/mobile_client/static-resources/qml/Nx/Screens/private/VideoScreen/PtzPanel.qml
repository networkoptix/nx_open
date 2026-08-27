// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.Mobile
import Nx.Mobile.Controls

import nx.vms.api

import "Ptz"
import "Ptz/joystick_utils.js" as JoystickUtils

Control
{
    id: control

    property PtzController controller
    property var presetViewModel

    property bool overlayStyle: false

    property alias customRotation: joystick.customRotation
    readonly property alias joystick: joystick

    readonly property alias zoomInPressed: zoomControl.zoomInPressed
    readonly property alias zoomOutPressed: zoomControl.zoomOutPressed
    readonly property alias moveDirection: joystick.direction
    readonly property alias focusInPressed: focusControl.focusInPressed
    readonly property alias focusOutPressed: focusControl.focusOutPressed

    signal autoFocusClicked()
    signal moveOnTapClicked()
    signal presetSwitchClicked()

    contentItem: ColumnLayout
    {
        id: content

        spacing: control.overlayStyle ? 16 : 20

        RowLayout
        {
            Layout.fillWidth: true
            spacing: 16

            FocusControl
            {
                id: focusControl

                visible: supportsFocusChanging || supportsAutoFocus
                overlayStyle: control.overlayStyle
                supportsFocusChanging: controller.capabilities & PtzAPI.Capability.continuousFocus
                supportsAutoFocus: controller.auxTraits & Ptz.ManualAutoFocusPtzTrait
                onFocusInPressedChanged: moveFocus(focusInPressed, 1)
                onFocusOutPressedChanged: moveFocus(focusOutPressed, -1)
                onAutoFocusClicked:
                {
                    controller.setAutoFocus()
                    control.autoFocusClicked()
                }

                Layout.alignment: Qt.AlignBottom

                function moveFocus(shouldMove, speed)
                {
                    var focusSpeed = shouldMove ? speed : 0
                    controller.continuousFocus(focusSpeed)
                }
            }

            ZoomControl
            {
                id: zoomControl

                visible: controller.capabilities & PtzAPI.Capability.continuousZoom
                overlayStyle: control.overlayStyle

                onZoomInPressedChanged: zoomMove(zoomInPressed, 0.5)
                onZoomOutPressedChanged: zoomMove(zoomOutPressed, -0.5)

                Layout.alignment: Qt.AlignBottom

                function zoomMove(shouldMove, speed)
                {
                    var zoomVector = shouldMove
                        ? Qt.vector3d(0, 0, speed)
                        : Qt.vector3d(0, 0, 0)

                    controller.continuousMove(zoomVector)
                }
            }

            Item
            {
                id: inlineSpacer

                Layout.fillWidth: true
                Layout.fillHeight: true

                LayoutItemProxy
                {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter

                    target: presetSwitch
                    visible: presetSwitch.inline && presetViewModel.hasPresets
                }
            }

            ColumnLayout
            {
                spacing: 8

                Item { Layout.fillHeight: true }

                PtzButton
                {
                    id: moveOnTapButton

                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    Layout.alignment: Qt.AlignRight

                    visible: controller.supportsMoveOnTap
                    overlayStyle: control.overlayStyle
                    radius: width / 2

                    icon.source: "image://skin/24x24/Outline/re_centre.svg"

                    onClicked: control.moveOnTapClicked()
                }

                Joystick
                {
                    id: joystick

                    readonly property vector2d zeroVector: Qt.vector2d(0, 0)
                    property vector2d movementVector: Qt.vector2d(0, 0)

                    overlayStyle: control.overlayStyle

                    Layout.alignment: Qt.AlignBottom

                    joystickType:
                    {
                        if (!visible)
                            return JoystickUtils.Type.Any

                        var caps = controller.capabilities
                        if (caps & PtzAPI.Capability.continuousPanTilt)
                        {
                            if (caps & Ptz.EightWayPtzTrait)
                                return JoystickUtils.Type.EightWayPtz
                            if (caps & Ptz.FourWayPtzTrait)
                                return JoystickUtils.Type.FourWayPtz

                            return JoystickUtils.Type.FreeWayPtz
                        }

                        if (caps & PtzAPI.Capability.continuousPan)
                            return JoystickUtils.Type.TwoWayHorizontal

                        if (caps & PtzAPI.Capability.continuousTilt)
                            return JoystickUtils.Type.TwoWayVertical

                        return JoystickUtils.Type.Any
                    }

                    visible: controller &&
                        (controller.capabilities & PtzAPI.Capability.continuousPan
                        || controller.capabilities & PtzAPI.Capability.continuousTilt)

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
        }

        Rectangle
        {
            id: separator

            color: ColorTheme.colors.dark12
            visible: !control.overlayStyle && presetViewModel.hasPresets

            Layout.fillWidth: true
            Layout.preferredHeight: 1
            Layout.leftMargin: -24
            Layout.rightMargin: -24
        }

        LayoutItemProxy
        {
            Layout.fillWidth: true

            target: presetSwitch
            visible: !presetSwitch.inline && presetViewModel.hasPresets
        }
    }

    PresetSwitch
    {
        id: presetSwitch

        readonly property bool inline: control.overlayStyle && implicitWidth <= inlineSpacer.width

        visible: presetViewModel.hasPresets
        overlayStyle: control.overlayStyle

        model: presetViewModel.presets
        currentIndex: presetViewModel.currentPresetIndex

        onClicked: control.presetSwitchClicked()
        onSelected: (index) => presetViewModel.setCurrentPreset(index)
    }
}
