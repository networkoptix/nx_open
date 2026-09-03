// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts
import QtQuick.Window

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Core.Effects
import Nx.Mobile.Controls

Rectangle
{
    id: speedControl

    // Selected speed. Speeds are internally rounded to the nearest power of two.
    property real speed: 1
    readonly property real effectiveSpeed: effectivePaused ? 0 : (forced1x ? 1 : speed)

    property alias displayedText: speedButton.text

    property real expandedWidth: 300
    readonly property real collapsedWidth: speedButton.width + horizontalMargins * 2
    property bool expanded: false
    property real expandCollapseDurationMs: 150
    property alias displayCollapseButton: speedControlCollapseButton.visible

    // Whether the control is locked to the 1x speed. Used in live playback mode.
    property bool forced1x: false
    property bool disabledWhenForced1x: false

    // Whether the control supports 0x speed position when paused.
    property bool pausable: true
    property bool paused: false
    readonly property bool effectivePaused: pausable && paused

    // Whether the control supports negative speeds.
    property bool reversible: true

    // These values must be the powers of two.
    // They will be rounded to the nearest power of two internally.
    property real minimumSpeed: 0.25
    property real maximumSpeed: 16
    property real maximumReverseSpeed: 16

    // Whether the slider is pressed and held by the user.
    property alias pressed: speedSlider.pressed

    // Whether the quick access menu is opened.
    readonly property alias menuOpened: quickAccessMenu.opened

    // A set of quick access speeds (via menu).
    // These values must be the powers of two.
    // They will be rounded to the nearest power of two internally.
    // This set will also be constrained by other constraints (e.g. `reversible`).
    property var quickAccessSpeeds: [-4, -2, -1, 0.5, 1, 2, 4, 8]

    // Margins between the quick access menu and the window edges.
    property real menuMargins: 12

    // The gap between the speed button and the menu popup.
    property real menuSpacing: 12

    // Additional margins.
    property real margins: 0
    property real horizontalMargins: margins
    property real verticalMargins: margins

    signal moved()

    implicitWidth: expanded ? expandedWidth : collapsedWidth
    implicitHeight: speedButton.height + verticalMargins * 2
    clip: true

    Behavior on implicitWidth { NumberAnimation { duration: expandCollapseDurationMs }}

    ControlButton
    {
        id: speedButton

        readonly property bool transparentControlMode: speedControl.color.a < 1.0

        suppressDisabledState: true
        anchors.left: speedControl.left
        anchors.leftMargin: speedControl.horizontalMargins
        anchors.verticalCenter: speedControl.verticalCenter
        width: 60

        text:
        {
            if (speedControl.effectiveSpeed == 0)
                return "0x"

            const sign = (speedControl.effectiveSpeed < 0) ? "-" : ""
            const value = Math.abs(speedControl.effectiveSpeed)

            return value >= 1
                ? `${sign}${Math.round(value)}x`
                : `${sign}1/${Math.round(1.0 / value)}x`
        }

        onClicked:
        {
            if (speedControl.expanded)
                quickAccessMenu.open()
            else
                speedControl.expanded = true
        }

        onPressAndHold:
            quickAccessMenu.open()

        Binding on backgroundColor
        {
            when: speedButton.transparentControlMode
            value: "transparent"
        }

        Rectangle
        {
            id: extraBackground

            visible: speedControl.menuOpened
            parent: speedButton.background
            anchors.fill: parent
            radius: speedButton.radius

            border.color: ColorTheme.colors.dark15

            color: speedButton.transparentControlMode
                ? ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
                : ColorTheme.colors.dark11
        }
    }

    RowLayout
    {
        id: expandedLayout

        anchors.left: speedButton.right
        anchors.verticalCenter: speedControl.verticalCenter
        spacing: 0

        height: speedControl.height - speedControl.verticalMargins * 2
        width: speedControl.expandedWidth - speedButton.width - speedControl.horizontalMargins * 2

        Slider
        {
            id: speedSlider

            property bool updating: false
            property real sliderSpeed: speedControl.effectivePaused ? 0 : speedControl.speed

            opacity: (speedControl.enabled && !enabled) ? 0.3 : 1.0
            enabled: !(speedControl.forced1x && speedControl.disabledWhenForced1x)

            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.rightMargin: expanded && !displayCollapseButton ? 18 : 0

            from: parameters.minSliderValue
            to: parameters.maxSliderValue
            stepSize: 1
            snapMode: Slider.SnapOnRelease
            grooveHighlight: Slider.FillingFromZero
            value: speedToValue(sliderSpeed)

            /**
             * JS object with speed to slider value conversion parameters:
             * `minExponent` - power-of-two exponent of minimum speed
             * `maxExponent` - power-of-two exponent of maximum forward speed
             * `maxReverseExponent` (optional) - power-of-two exponent of maximum backward speed
             * `minSpeed` - adjusted minimum speed
             * `maxSpeed` - adjusted maximum forward speed
             * `maxReverseSpeed` - adjusted maximum backward speed
             * `minSliderValue` - minimum linear value for the slider
             * `maxSliderValue` - maximum linear value for the slider
             * `unitSpeedValue` - 0 or 1, depending on pausability
             */
            readonly property var parameters:
            {
                let result = {}
                const reversible = speedControl.reversible && speedControl.maximumReverseSpeed != 0

                // Minimum speed should not be greater than 1x.
                result.minSpeed = Math.min(1, Math.abs(speedControl.minimumSpeed))

                // Maximum speed should not be lesser than 1x or `minSpeed`.
                result.maxSpeed = Math.max(1, result.minSpeed, Math.abs(speedControl.maximumSpeed))

                result.minExponent = Math.round(Math.log2(result.minSpeed))
                result.maxExponent = Math.round(Math.log2(result.maxSpeed))
                result.unitSpeedValue = speedControl.pausable ? 1 : 0

                result.maxSliderValue = result.maxExponent - result.minExponent
                    + result.unitSpeedValue

                if (reversible)
                {
                    result.maxReverseSpeed = Math.max(
                        1, result.minSpeed, Math.abs(speedControl.maximumReverseSpeed))

                    result.maxReverseExponent = Math.round(Math.log2(result.maxReverseSpeed))
                    result.minSliderValue = -(result.maxReverseExponent - result.minExponent + 1)
                }
                else
                {
                    result.maxReverseSpeed = 0
                    result.minSliderValue = 0
                }

                return result
            }

            function speedToValue(speed)
            {
                if (speed > 0)
                {
                    const boundedPower = MathUtils.bound(parameters.minExponent,
                        Math.round(Math.log2(speed)), parameters.maxExponent)

                    return parameters.unitSpeedValue + boundedPower - parameters.minExponent
                }

                if (speed == 0 || parameters.maxReverseExponent === undefined)
                    return 0

                const boundedReversePower = MathUtils.bound(parameters.minExponent,
                    Math.round(Math.log2(-speed)), parameters.maxReverseExponent)

                return -1 - (boundedReversePower - parameters.minExponent)
            }

            function valueToSpeed(value)
            {
                value = MathUtils.bound(
                    parameters.minSliderValue, value, parameters.maxSliderValue)

                if (value >= parameters.unitSpeedValue)
                    return 2.0 ** (value - parameters.unitSpeedValue + parameters.minExponent)

                if (value == 0 || parameters.maxReverseExponent === undefined)
                    return 0.0

                return -(2.0 ** (-value - 1 + parameters.minExponent))
            }

            onSliderSpeedChanged:
            {
                if (!updating)
                    value = speedToValue(sliderSpeed)
            }

            onMoved:
            {
                speedSlider.updating = true
                speedControl.speed = valueToSpeed(speedSlider.value)
                speedControl.moved()
                speedSlider.updating = false
            }
        }

        ControlButton
        {
            id: speedControlCollapseButton

            leftPadding: 18
            rightPadding: 18
            icon.source: "image://skin/24x24/Solid/cancel.svg"
            Layout.alignment: Qt.AlignVCenter

            Binding on backgroundColor
            {
                when: speedControl.color.a < 1.0
                value: "transparent"
            }

            onClicked:
                speedControl.expanded = false
        }
    }

    onEffectivePausedChanged:
    {
        if (!effectivePaused && speed == 0)
            speed = 1
    }

    Menu
    {
        id: quickAccessMenu

        readonly property int dimmedEdges:
        {
            let result = 0
            if (!contentItem.atYBeginning)
                result |= Qt.TopEdge
            if (!contentItem.atYEnd)
                result |= Qt.BottomEdge
            return result
        }

        parent: speedButton
        margins: speedControl.menuMargins

        onAboutToShow:
            updatePosition()

        function updatePosition()
        {
            const windowItem = speedControl.Window.window.contentItem
            const parentTop = parent.mapToItem(windowItem, 0, 0).y
            const parentBottom = parent.mapToItem(windowItem, 0, parent.height).y

            const spaceAbove = parentTop - windowItem.SafeArea.margins.top
            const spaceBelow = windowItem.height - windowItem.SafeArea.margins.bottom - parentBottom

            const margin = quickAccessMenu.margins
            const gap = speedControl.menuSpacing

            if (spaceBelow > spaceAbove)
            {
                height = Math.min(implicitHeight, spaceBelow - margin - gap)
                y = parent.height + gap
            }
            else
            {
                height = Math.min(implicitHeight, spaceAbove - margin - gap)
                y = -(height + gap)
            }
        }

        Repeater
        {
            model: speedControl.quickAccessSpeeds.reduce(
                (result, value) =>
                {
                    const absSpeed = 2.0 ** Math.round(Math.log2(Math.abs(value)))
                    if (absSpeed >= speedSlider.parameters.minSpeed)
                    {
                        if (value < 0 && absSpeed <= speedSlider.parameters.maxReverseSpeed)
                            result.push(-absSpeed)
                        else if (value > 0 && absSpeed <= speedSlider.parameters.maxSpeed)
                            result.push(absSpeed)
                    }

                    return result
                },
                [])

            MenuItem
            {
                id: menuItem

                readonly property real speed: modelData

                text: `${menuItem.speed}x`
                checkable: false
                showIndicator: true
                checked: menuItem.speed === speedControl.effectiveSpeed
                enabled: !speedControl.forced1x || menuItem.speed === 1 || menuItem.speed < 0
                showDisabled: true

                onTriggered:
                {
                    speedControl.speed = menuItem.speed
                    speedControl.moved()
                }
            }
        }

        Component
        {
            id: effectComponent

            EdgeOpacityGradient
            {
                edges: quickAccessMenu.dimmedEdges
                gradientWidth: 32
            }
        }

        Component.onCompleted:
        {
            contentItem.layer.effect = effectComponent
            contentItem.layer.enabled = Qt.binding(() => !!quickAccessMenu.dimmedEdges)
        }
    }

    Connections
    {
        target: speedControl.Window.window

        function onHeightChanged() { quickAccessMenu.close() }
        function onWidthChanged() { quickAccessMenu.close() }
    }
}
