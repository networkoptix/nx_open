// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Mobile.Controls

Rectangle
{
    id: speedControl

    // Selected speed. Speeds are internally rounded to the nearest power of two.
    property real speed: 1
    readonly property real effectiveSpeed: (pausable && paused) ? 0 : (forced1x ? 1 : speed)

    property alias displayedText: speedButton.text

    property real expandedWidth: 300
    readonly property real collapsedWidth: speedButton.width
    property bool expanded: false
    property real expandCollapseDurationMs: 150

    // Whether the control is locked to the 1x speed. Used in live playback mode.
    property bool forced1x: false

    // Whether the control supports 0x speed position when paused.
    property bool pausable: true
    property bool paused: false

    // Whether the control supports negative speeds.
    property bool reversible: true

    // These values must be the powers of two.
    // They will be rounded to the nearest power of two internally.
    property real minimumSpeed: 0.25
    property real maximumSpeed: 16
    property real maximumReverseSpeed: 16

    signal moved()

    width: expanded ? expandedWidth : collapsedWidth
    height: speedButton.height
    clip: true

    Behavior on width { NumberAnimation { duration: expandCollapseDurationMs }}

    ControlButton
    {
        id: speedButton

        enabled: !speedControl.expanded
        suppressDisabledState: true
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
            speedControl.expanded = true
    }

    RowLayout
    {
        id: expandedLayout

        anchors.left: speedButton.right
        spacing: 0

        height: speedControl.height
        width: speedControl.expandedWidth - speedButton.width

        Slider
        {
            id: speedSlider

            property bool updating: false

            opacity: (speedControl.enabled && !enabled) ? 0.3 : 1.0
            enabled: !speedControl.forced1x

            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true

            from: parameters.minSliderValue
            to: parameters.maxSliderValue
            stepSize: 1
            snapMode: Slider.SnapOnRelease
            grooveHighlight: Slider.FillingFromZero

            /**
             * JS object with speed to slider value conversion parameters:
             * `minExponent` - power-of-two exponent of minimum speed
             * `maxExponent` - power-of-two exponent of maximum forward speed
             * `maxReverseExponent` (optional) - power-of-two exponent of maximum backward speed
             * `minSliderValue` - minimum linear value for the slider
             * `maxSliderValue` - maximum linear value for the slider
             * `unitSpeedValue` - 0 or 1, depending on pausability
             */
            readonly property var parameters:
            {
                let result = {}
                const reversible = speedControl.reversible && speedControl.maximumReverseSpeed != 0

                // Minimum speed should not be greater than 1x.
                const minSpeed = Math.min(1, Math.abs(speedControl.minimumSpeed))

                // Maximum speed should not be lesser than 1x or `minSpeed`.
                const maxSpeed = Math.max(1, minSpeed, Math.abs(speedControl.maximumSpeed))

                result.minExponent = Math.round(Math.log2(minSpeed))
                result.maxExponent = Math.round(Math.log2(maxSpeed))
                result.unitSpeedValue = speedControl.pausable ? 1 : 0

                result.maxSliderValue = result.maxExponent - result.minExponent
                    + result.unitSpeedValue

                if (reversible)
                {
                    const maxReverseSpeed = Math.max(
                        1, minSpeed, Math.abs(speedControl.maximumReverseSpeed))

                    result.maxReverseExponent = Math.round(Math.log2(maxReverseSpeed))

                    result.minSliderValue = -(result.maxReverseExponent - result.minExponent + 1)
                }
                else
                {
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

            onClicked:
                speedControl.expanded = false
        }
    }

    onPausedChanged:
    {
        if (!paused && speed == 0)
            speed = 1
    }

    onEffectiveSpeedChanged:
    {
        if (!speedSlider.updating)
            speedSlider.value = speedSlider.speedToValue(effectiveSpeed)
    }
}
