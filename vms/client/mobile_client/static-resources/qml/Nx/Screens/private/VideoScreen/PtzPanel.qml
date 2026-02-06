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

    property alias preloaders: preloadersPanel

    readonly property PtzController controller: PtzController
    {
        property bool supportsPresets: capabilities & PtzAPI.Capability.presets
        property bool supportsMoveOnTap: capabilities & PtzAPI.Capability.viewport
    }

    property alias customRotation: joystick.customRotation
    property alias moveOnTapMode: moveOnTapButton.checked
    readonly property alias joystick: joystick

    function moveViewport(viewportRect, aspect)
    {
        controller.viewportMove(aspect, viewportRect, 1)
    }

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
            function onAutoFocusClicked()
            {
                preloadersPanel.showCommonPreloader()
            }
        }
        Connections
        {
            target: presets

            function onSelected()
            {
                preloadersPanel.showCommonPreloader()
            }
        }
    }

    contentItem: ColumnLayout
    {
        id: content

        spacing: 0

        RowLayout
        {
            Layout.fillWidth: true
            spacing: 16

            FocusControl
            {
                id: focusControl

                visible: supportsFocusChanging || supportsAutoFocus
                supportsFocusChanging: controller.capabilities & PtzAPI.Capability.continuousFocus
                supportsAutoFocus: controller.auxTraits & Ptz.ManualAutoFocusPtzTrait
                onFocusInPressedChanged: moveFocus(focusInPressed, 1)
                onFocusOutPressedChanged: moveFocus(focusOutPressed, -1)
                onAutoFocusClicked: controller.setAutoFocus()

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

            Item { Layout.fillWidth: true }

            ColumnLayout
            {
                spacing: 8

                Item { Layout.fillHeight: true }

                Button
                {
                    id: moveOnTapButton

                    Layout.preferredWidth: 44
                    Layout.preferredHeight: 44
                    Layout.alignment: Qt.AlignRight

                    visible: controller.supportsMoveOnTap
                    radius: width / 2
                    padding: 0
                    checkable: true
                    checked: false

                    type: Button.Type.LightInterface
                    icon.source: "image://skin/24x24/Outline/ptz.svg"
                    icon.width: 24
                    icon.height: 24
                }

                Joystick
                {
                    id: joystick

                    readonly property vector2d zeroVector: Qt.vector2d(0, 0)
                    property vector2d movementVector: Qt.vector2d(0, 0)

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
            color: ColorTheme.colors.dark12
            implicitHeight: 1
            Layout.fillWidth: true
            Layout.topMargin: 20
            Layout.leftMargin: -20
            Layout.rightMargin: -20
        }

        Presets
        {
            id: presets

            Layout.fillWidth: true
            Layout.preferredHeight: 64
            Layout.leftMargin: -20
            Layout.rightMargin: -20

            resource: controller && controller.resource
            currentIndex: controller.activePresetIndex
            visible: controller.presetsCount && controller.supportsPresets

            Timer
            {
                id: skipIndexChangedNotificationTimer

                interval: 10000
            }

            onSelected: (index) =>
            {
                if (currentIndex == -1)
                    return

                controller.setPresetByIndex(index)
                skipIndexChangedNotificationTimer.restart()
            }

            Connections
            {
                target: controller
                function onActivePresetIndexChanged()
                {
                    if (!skipIndexChangedNotificationTimer.running)
                        presets.currentIndex = controller.activePresetIndex
                }
            }
        }
    }
}
