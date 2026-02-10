// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Shapes

import nx.vms.client.core.timeline as Timeline

Item
{
    id: item

    enum Direction { Upward, Downward }
    property int direction: VerticalTimeScale.Upward

    property alias startTimeMs: d.startTimeMs
    property alias durationMs: d.durationMs
    readonly property real millisecondsPerPixel: durationMs / Math.max(height, 1)

    property alias timeZone: d.timeZone

    property color majorTickColor: item.textColor
    property real majorTickLength: 8

    property color minorTickColor: majorTickColor
    property real minorTickLength: 4
    property real minorTickOffset: 4

    property real minimumTickSpacing: 12

    property color textColor: "grey"
    property font font

    property alias locale: labelFormatter.locale

    property real spacing: 4

    property real labelOffset: Math.max(majorTickLength, minorTickLength + minorTickOffset)
        + spacing

    property int fadeDurationMs: 150

    readonly property alias labelFormatter: labelFormatter

    readonly property alias zoomLevel: d.majorTicksLevel

    property Component labelDelegate: Component
    {
        Text
        {
            id: label

            required property var modelData

            font: item.font
            color: item.textColor
            text: item.labelFormatter.tickLabel(modelData.timestampMs, d.timeZone, item.zoomLevel)
            horizontalAlignment: Qt.AlignRight
        }
    }

    function timeToPosition(timeMs)
    {
        const pos = (timeMs - startTimeMs) / millisecondsPerPixel
        return direction === VerticalTimeScale.Upward ? (height - pos) : pos
    }

    function positionToTime(y)
    {
        const pos = direction === VerticalTimeScale.Upward ? (height - y) : y
        return startTimeMs + pos * millisecondsPerPixel
    }

    Shape
    {
        id: shape

        x: item.LayoutMirroring.enabled ? 0 : item.width
        height: item.parentHeight

        transform: Scale { xScale: item.LayoutMirroring.enabled ? 1 : -1 }

        ShapePath
        {
            id: majorTicks

            strokeColor: majorTickColor
            strokeWidth: 1
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            pathHints: ShapePath.PathLinear

            PathMultiline
            {
                paths: d.majorTicks.map(
                    (tickMs) =>
                    {
                        const y = item.timeToPosition(tickMs)
                        return [Qt.point(0.0, y), Qt.point(item.majorTickLength, y)]
                    })
            }
        }

        ShapePath
        {
            id: minorTicks

            strokeColor: minorTickColor
            strokeWidth: 1
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap
            pathHints: ShapePath.PathLinear

            PathMultiline
            {
                paths: d.minorTicks.map(
                    (tickMs) =>
                    {
                        const y = item.timeToPosition(tickMs)
                        return [Qt.point(item.minorTickOffset, y),
                            Qt.point(item.minorTickOffset + item.minorTickLength, y)]
                    })
            }
        }
    }

    Component
    {
        id: labelHolderComponent

        Item
        {
            id: labelHolder

            property var modelData
            property Item label

            width: label?.width ?? 0
            height: label?.height ?? 0

            anchors.right: item.right
            anchors.rightMargin: item.labelOffset

            y: item.timeToPosition(modelData.timestampMs) - height / 2

            opacity: 0 //< Default state means the label is pooled or just created.

            states: [
                State
                {
                    name: "AddedInstantly"
                    PropertyChanges { target: labelHolder; opacity: 1 }
                },

                State
                {
                    name: "Added"
                    PropertyChanges { target: labelHolder; opacity: 1 }
                },

                State
                {
                    name: "Removed"
                    PropertyChanges { target: labelHolder; opacity: 0 }
                }
            ]

            transitions: [
                Transition
                {
                    to: "Added"
                    OpacityAnimator { duration: item.fadeDurationMs }
                },

                Transition
                {
                    to: "Removed"
                    OpacityAnimator { duration: item.fadeDurationMs }
                }
            ]
        }
    }

    Timeline.ZoomCalculator
    {
        id: d

        minimumTickSpacingMs: item.minimumTickSpacing * item.millisecondsPerPixel

        property var labels: new Map()
        property var pooledLabels: []

        property real prevStartTimeMs: 0
        property real prevDurationMs: 0

        onMajorTicksChanged:
        {
            let newTicks = {}
            for (const timeMs of d.majorTicks)
                newTicks[timeMs] = 1

            const oldTicks = [...d.labels.keys()]

            const isInserted = (timeMs) => (oldTicks.length > 0 && timeMs >= d.prevStartTimeMs
                && timeMs < (d.prevStartTimeMs + d.prevDurationMs))

            for (const timeMs of oldTicks)
            {
                const label = d.labels.get(timeMs)
                console.assert(label, `${label} label for ${timeMs}`)

                if (newTicks[timeMs])
                {
                    label.state = isInserted(timeMs) ? "Added" : "AddedInstantly"
                    newTicks[timeMs] = 0
                }
                else if (label.y >= item.height || label.y + label.height <= 0)
                {
                    d.releaseLabel(label)
                }
                else
                {
                    label.state = "Removed"
                }
            }

            for (const timeMs in newTicks)
            {
                if (newTicks[timeMs])
                {
                    console.assert(!d.labels.has(timeMs))

                    const modelData = {timestampMs: timeMs}
                    ensureLabel(modelData).state = isInserted(timeMs) ? "Added" : "AddedInstantly"
                }
            }

            d.prevStartTimeMs = d.startTimeMs
            d.prevDurationMs = d.durationMs
        }

        function ensureLabel(modelData)
        {
            const existing = pooledLabels.pop()
            if (existing)
            {
                existing.modelData = modelData
                existing.visible = true
                d.labels.set(modelData.timestampMs, existing)
                return existing
            }

            const labelHolder = labelHolderComponent.createObject(item, {modelData: modelData})
            if (item.labelDelegate)
            {
                labelHolder.label = item.labelDelegate.createObject(
                    labelHolder, {modelData: modelData})

                labelHolder.label.modelData = Qt.binding(() => labelHolder.modelData)
            }

            d.labels.set(modelData.timestampMs, labelHolder)
            return labelHolder
        }

        function releaseLabel(labelHolder)
        {
            labelHolder.visible = false
            labelHolder.state = ""
            pooledLabels.push(labelHolder)
            d.labels.delete(labelHolder.modelData.timestampMs)
        }
    }

    Timeline.LabelFormatter
    {
        id: labelFormatter
    }
}
