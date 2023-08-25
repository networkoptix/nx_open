// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQml

import Nx
import Nx.Core
import Nx.Controls
import Nx.Instruments

import nx.vms.client.core
import nx.vms.client.desktop

import "../figure_utils.js" as F

Figure
{
    id: figure

    readonly property bool hasFigure: true

    acceptable: minRect.width <= maxRect.width && minRect.height <= maxRect.height

    readonly property rect minRect: Qt.rect(
        Math.min(d.minP1.x, d.minP2.x),
        Math.min(d.minP1.y, d.minP2.y),
        Math.abs(d.minP1.x - d.minP2.x),
        Math.abs(d.minP1.y - d.minP2.y))

    readonly property rect maxRect: Qt.rect(
        Math.min(d.maxP1.x, d.maxP2.x),
        Math.min(d.maxP1.y, d.maxP2.y),
        Math.abs(d.maxP1.x - d.maxP2.x),
        Math.abs(d.maxP1.y - d.maxP2.y))

    property color minBoxColor: ColorTheme.colors.roi.minBox
    property color maxBoxColor: ColorTheme.colors.roi.maxBox

    MouseArea
    {
        id: mouseArea

        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        hoverEnabled: enabled

        cursorShape: drag.target ? Qt.SizeAllCursor : Qt.ArrowCursor

        property Item dragBox

        drag.onActiveChanged:
        {
            if (drag.active)
            {
                drag.minimumX = -dragBox.x
                drag.minimumY = -dragBox.y
                drag.maximumX = drag.minimumX + width - dragBox.width
                drag.maximumY = drag.minimumY + height - dragBox.height
            }
            else
            {
                var offset = Qt.point(F.relX(drag.target.x, figure), F.relY(drag.target.y, figure))
                drag.target.x = 0
                drag.target.y = 0

                if (dragBox === minBox)
                {
                    var size = Qt.size(minRect.width, minRect.height)
                    d.minP1 = Qt.point(minRect.x + offset.x, minRect.y + offset.y)
                    d.minP2 = Qt.point(d.minP1.x + size.width, d.minP1.y + size.height)
                }
                else if (dragBox === maxBox)
                {
                    var size = Qt.size(maxRect.width, maxRect.height)
                    d.maxP1 = Qt.point(maxRect.x + offset.x, maxRect.y + offset.y)
                    d.maxP2 = Qt.point(d.maxP1.x + size.width, d.maxP1.y + size.height)
                }
            }
        }

        onPositionChanged:
        {
            if (pressed)
            {
                if (dragBox)
                    dragBox.updateTextPlacement()
                return
            }

            if (minBox.contains(Qt.point(mouseX - minBox.x, mouseY - minBox.y)))
                setDragBox(minBox)
            else if (maxBox.contains(Qt.point(mouseX - maxBox.x, mouseY - maxBox.y)))
                setDragBox(maxBox)
            else
                setDragBox(null)
        }

        function setDragBox(box)
        {
            dragBox = box
            drag.target = box && box.parent
        }
    }

    Item
    {
        id: maxDraggable

        width: parent.width
        height: parent.height

        Rectangle
        {
            id: maxBox

            border.width: 2
            border.color: maxBoxColor
            color: ColorTheme.transparent(maxBoxColor, 0.3)

            x: F.absX(maxRect.x, figure) - 1
            y: F.absY(maxRect.y, figure) - 1
            width: F.absX(maxRect.width, figure) + 2
            height: F.absY(maxRect.height, figure) + 2

            function updateTextPlacement()
            {
                maxText.update()
            }

            BoundLabel
            {
                id: maxText

                boundingItem: figure
                preferTopLeftPosition: true
                revertRotationAngle: figure.parent.parent.rotation

                label.text: qsTr("MAX")
                label.color: maxBoxColor
                label.style: Text.Outline
                label.styleColor: "black"
                label.font.bold: true
            }
        }

        PointGrip
        {
            id: maxTopLeftGrip

            color: maxBoxColor
            z: (hovered || dragging) ? 3 : 1

            onXChanged: dragging && (d.maxP1.x = F.relX(x, figure))
            onYChanged: dragging && (d.maxP1.y = F.relY(y, figure))

            Binding
            {
                target: maxTopLeftGrip
                property: "x"
                value: F.absX(d.maxP1.x, figure)
                when: !maxTopLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }

            Binding
            {
                target: maxTopLeftGrip
                property: "y"
                value: F.absY(d.maxP1.y, figure)
                when: !maxTopLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: maxBottomRightGrip

            color: maxBoxColor
            z: (hovered || dragging) ? 3 : 1

            onXChanged: dragging && (d.maxP2.x = F.relX(x, figure))
            onYChanged: dragging && (d.maxP2.y = F.relY(y, figure))

            Binding
            {
                target: maxBottomRightGrip
                property: "x"
                value: F.absX(d.maxP2.x, figure)
                when: !maxBottomRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }

            Binding
            {
                target: maxBottomRightGrip
                property: "y"
                value: F.absY(d.maxP2.y, figure)
                when: !maxBottomRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: maxTopRightGrip

            color: maxBoxColor
            z: (hovered || dragging) ? 3 : 1

            onXChanged: dragging && (d.maxP2.x = F.relX(x, figure))
            onYChanged: dragging && (d.maxP1.y = F.relY(y, figure))

            Binding
            {
                target: maxTopRightGrip
                property: "x"
                value: F.absX(d.maxP2.x, figure)
                when: !maxTopRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }

            Binding
            {
                target: maxTopRightGrip
                property: "y"
                value: F.absY(d.maxP1.y, figure)
                when: !maxTopRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: maxBottomLeftGrip

            color: maxBoxColor
            z: (hovered || dragging) ? 3 : 1

            onXChanged: dragging && (d.maxP1.x = F.relX(x, figure))
            onYChanged: dragging && (d.maxP2.y = F.relY(y, figure))

            Binding
            {
                target: maxBottomLeftGrip
                property: "x"
                value: F.absX(d.maxP1.x, figure)
                when: !maxBottomLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }

            Binding
            {
                target: maxBottomLeftGrip
                property: "y"
                value: F.absY(d.maxP2.y, figure)
                when: !maxBottomLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: maxLeftGrip

            color: maxBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeHorCursor
            axis: Drag.XAxis
            y: (maxTopLeftGrip.y + maxBottomLeftGrip.y) / 2
            z: (hovered || dragging) ? 2 : 0

            onXChanged: dragging && (d.maxP1.x = F.relX(x, figure))

            Binding
            {
                target: maxLeftGrip
                property: "x"
                value: F.absX(d.maxP1.x, figure)
                when: !maxLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: maxRightGrip

            color: maxBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeHorCursor
            axis: Drag.XAxis
            y: maxLeftGrip.y
            z: (hovered || dragging) ? 2 : 0

            onXChanged: dragging && (d.maxP2.x = F.relX(x, figure))

            Binding
            {
                target: maxRightGrip
                property: "x"
                value: F.absX(d.maxP2.x, figure)
                when: !maxRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: maxTopGrip

            color: maxBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeVerCursor
            axis: Drag.YAxis
            x: (maxTopLeftGrip.x + maxTopRightGrip.x) / 2
            z: (hovered || dragging) ? 2 : 0

            onYChanged: dragging && (d.maxP1.y = F.relY(y, figure))

            Binding
            {
                target: maxTopGrip
                property: "y"
                value: F.absY(d.maxP1.y, figure)
                when: !maxTopGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: maxBottomGrip

            color: maxBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeVerCursor
            axis: Drag.YAxis
            x: maxTopGrip.x
            z: (hovered || dragging) ? 2 : 0

            onYChanged: dragging && (d.maxP2.y = F.relY(y, figure))

            Binding
            {
                target: maxBottomGrip
                property: "y"
                value: F.absY(d.maxP2.y, figure)
                when: !maxBottomGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }
    }

    Item
    {
        id: minDraggable

        width: parent.width
        height: parent.height

        Rectangle
        {
            id: minBox

            border.width: 2
            border.color: minBoxColor
            color: ColorTheme.transparent(minBoxColor, 0.3)

            x: F.absX(minRect.x, figure) - 1
            y: F.absY(minRect.y, figure) - 1
            width: F.absX(minRect.width, figure) + 2
            height: F.absY(minRect.height, figure) + 2

            function updateTextPlacement()
            {
                minText.update()
            }

            BoundLabel
            {
                id: minText

                boundingItem: figure
                revertRotationAngle: figure.parent.parent.rotation

                label.text: qsTr("MIN")
                label.color: minBoxColor
                label.style: Text.Outline
                label.styleColor: "black"
                label.font.bold: true
            }
        }

        PointGrip
        {
            id: minTopLeftGrip

            color: minBoxColor
            z: (hovered || dragging) ? 3 : 1

            onXChanged: dragging && (d.minP1.x = F.relX(x, figure))
            onYChanged: dragging && (d.minP1.y = F.relY(y, figure))

            Binding
            {
                target: minTopLeftGrip
                property: "x"
                value: F.absX(d.minP1.x, figure)
                when: !minTopLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }

            Binding
            {
                target: minTopLeftGrip
                property: "y"
                value: F.absY(d.minP1.y, figure)
                when: !minTopLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: minBottomRightGrip

            color: minBoxColor
            z: (hovered || dragging) ? 3 : 1

            onXChanged: dragging && (d.minP2.x = F.relX(x, figure))
            onYChanged: dragging && (d.minP2.y = F.relY(y, figure))

            Binding
            {
                target: minBottomRightGrip
                property: "x"
                value: F.absX(d.minP2.x, figure)
                when: !minBottomRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }

            Binding
            {
                target: minBottomRightGrip
                property: "y"
                value: F.absY(d.minP2.y, figure)
                when: !minBottomRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: minTopRightGrip

            color: minBoxColor
            z: (hovered || dragging) ? 3 : 1

            onXChanged: dragging && (d.minP2.x = F.relX(x, figure))
            onYChanged: dragging && (d.minP1.y = F.relY(y, figure))

            Binding
            {
                target: minTopRightGrip
                property: "x"
                value: F.absX(d.minP2.x, figure)
                when: !minTopRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }

            Binding
            {
                target: minTopRightGrip
                property: "y"
                value: F.absY(d.minP1.y, figure)
                when: !minTopRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: minBottomLeftGrip

            color: minBoxColor
            z: (hovered || dragging) ? 3 : 1

            onXChanged: dragging && (d.minP1.x = F.relX(x, figure))
            onYChanged: dragging && (d.minP2.y = F.relY(y, figure))

            Binding
            {
                target: minBottomLeftGrip
                property: "x"
                value: F.absX(d.minP1.x, figure)
                when: !minBottomLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }

            Binding
            {
                target: minBottomLeftGrip
                property: "y"
                value: F.absY(d.minP2.y, figure)
                when: !minBottomLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: minLeftGrip

            color: minBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeHorCursor
            axis: Drag.XAxis
            y: (minTopLeftGrip.y + minBottomLeftGrip.y) / 2
            z: (hovered || dragging) ? 2 : 0

            onXChanged: dragging && (d.minP1.x = F.relX(x, figure))

            Binding
            {
                target: minLeftGrip
                property: "x"
                value: F.absX(d.minP1.x, figure)
                when: !minLeftGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: minRightGrip

            color: minBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeHorCursor
            axis: Drag.XAxis
            y: minLeftGrip.y
            z: (hovered || dragging) ? 2 : 0

            onXChanged: dragging && (d.minP2.x = F.relX(x, figure))

            Binding
            {
                target: minRightGrip
                property: "x"
                value: F.absX(d.minP2.x, figure)
                when: !minRightGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: minTopGrip

            color: minBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeVerCursor
            axis: Drag.YAxis
            x: (minTopLeftGrip.x + minTopRightGrip.x) / 2
            z: (hovered || dragging) ? 2 : 0

            onYChanged: dragging && (d.minP1.y = F.relY(y, figure))

            Binding
            {
                target: minTopGrip
                property: "y"
                value: F.absY(d.minP1.y, figure)
                when: !minTopGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }

        PointGrip
        {
            id: minBottomGrip

            color: minBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeVerCursor
            axis: Drag.YAxis
            x: minTopGrip.x
            z: (hovered || dragging) ? 2 : 0

            onYChanged: dragging && (d.minP2.y = F.relY(y, figure))

            Binding
            {
                target: minBottomGrip
                property: "y"
                value: F.absY(d.minP2.y, figure)
                when: !minBottomGrip.dragging
                restoreMode: Binding.RestoreNone
            }
        }
    }

    hint:
    {
        if (d.dragging)
            return null

        return acceptable
            ? qsTr("Set minimum and maximum object size.")
            : qsTr("Minimum object size cannot be greater than maximum.")
    }

    hintStyle: acceptable ? Banner.Style.Info : Banner.Style.Error

    hoverInstrument: Instrument { item: mouseArea }

    // TODO: #vkutin #dklychkov Avoid code duplication with ObjectSizeConstraints.qml

    function deserialize(value)
    {
        const kDefaultMinSize = 0
        const kDefaultMaxSize = 1

        var minimum = deserializeSize(value && value.minimum, kDefaultMinSize, Qt.size(0, 0))
        var maximum = deserializeSize(value && value.maximum, kDefaultMaxSize, minimum)

        var minBoxPosition = deserializePosition(
            value && Array.isArray(value.positions) && value.positions[0], minimum)

        var maxBoxPosition = deserializePosition(
            value && Array.isArray(value.positions) && value.positions[1], maximum)

        d.minP1 = minBoxPosition
        d.minP2 = Qt.point(d.minP1.x + minimum.width, d.minP1.y + minimum.height)
        d.maxP1 = maxBoxPosition
        d.maxP2 = Qt.point(d.maxP1.x + maximum.width, d.maxP1.y + maximum.height)
    }

    function serialize()
    {
        return {
            "minimum": [minRect.width, minRect.height],
            "maximum": [maxRect.width, maxRect.height],
            "positions": [
                [minRect.x, minRect.y],
                [maxRect.x, maxRect.y]
            ]
        }
    }

    function deserializeSize(sizeJson, defaultSize, minimumSize)
    {
        var size = Array.isArray(sizeJson)
            ? Qt.size(sizeJson[0] || defaultSize, sizeJson[1] || defaultSize)
            : Qt.size(defaultSize, defaultSize)

        size.width = MathUtils.bound(minimumSize.width, size.width, 1)
        size.height = MathUtils.bound(minimumSize.height, size.height, 1)

        return size
    }

    function deserializePosition(positionJson, size)
    {
        var defaultPos = Qt.point((1 - size.width) / 2, (1 - size.height) / 2)
        if (!Array.isArray(positionJson))
            return defaultPos

        var pos = Qt.point(
            Utils.getValue(positionJson[0], defaultPos.x),
            Utils.getValue(positionJson[1], defaultPos.y))

        return (pos.x >= 0 && pos.x + size.width <= 1 && pos.y >= 0 && pos.y + size.height <= 1)
            ? pos
            : defaultPos
    }

    QtObject
    {
        id: d

        // Diagonal points of minimum object size rectangle (any two diagonal points).
        property point minP1
        property point minP2

        // Diagonal points of maximum object size rectangle (any two diagonal points).
        property point maxP1
        property point maxP2

        readonly property bool dragging: mouseArea.drag.active
            || maxTopLeftGrip.dragging
            || maxTopRightGrip.dragging
            || maxBottomRightGrip.dragging
            || maxBottomLeftGrip.dragging
            || maxLeftGrip.dragging
            || maxRightGrip.dragging
            || maxTopGrip.dragging
            || maxBottomGrip.dragging
            || minTopLeftGrip.dragging
            || minTopRightGrip.dragging
            || minBottomRightGrip.dragging
            || minBottomLeftGrip.dragging
            || minLeftGrip.dragging
            || minRightGrip.dragging
            || minTopGrip.dragging
            || minBottomGrip.dragging
    }
}
