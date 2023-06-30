// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx.Core 1.0

/**
 * Adaptive transmission helper: each time its `transform` function is called, the transmission
 * factor is increased by `factorDelta` value until `maximumFactor` is reached. If the function
 * isn't called for `decayDelayMs`, the transmission factor starts to decay - decrease back to
 * the initial value.
 */
NxObject
{
    id: controller

    /** Initial transmission factor. */
    property real initialFactor: 1.0

    /** Current transmission factor. */
    readonly property alias currentFactor: d.currentFactor

    /** Maximum transmission factor. */
    property real maximumFactor: 10.0

    /** A delta added to the current transmission factor upon each `transmit` call. */
    property real factorDelta: 1.0

    /** A delay after which the current transmission factor starts to decrease. */
    property int decayDelayMs: 100

    /** Decay curve type (Easing.Type). */
    property int decayType: Easing.InQuad

    /* Full decay (from maximumFactor to initialFactor) decay duration. */
    property int fullDecayDurationMs: 1000

    /* Decay speed in factor units per millisecond. */
    property real decayPerMs: (controller.maximumFactor - controller.initialFactor)
        * controller.fullDecayDurationMs

    /** Adaptive transmission function. */
    function transform(value)
    {
        const processedValue = value * d.currentFactor
        d.currentFactor = Math.min(d.currentFactor + factorDelta, controller.maximumFactor)
        d.decayDurationMs = (d.currentFactor - controller.initialFactor) / decayPerMs
        decay.restart()
        return processedValue
    }

    // Implementation.

    NxObject
    {
        id: d

        property real currentFactor: initialFactor
        property alias decayDurationMs: decayAnimation.duration

        SequentialAnimation
        {
            id: decay

            PauseAnimation
            {
                duration: decayDelayMs
            }

            NumberAnimation
            {
                id: decayAnimation

                target: d
                property: "currentFactor"
                to: controller.initialFactor
                easing.type: controller.decayType
            }
        }
    }
}
