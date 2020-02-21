import QtQuick 2.0

import Nx 1.0
import Nx.Instruments 1.0

import nx.vms.client.core 1.0
import nx.client.desktop 1.0

import "../figure_utils.js" as F

Figure
{
    id: figure

    readonly property bool hasFigure: true

    acceptable: true

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

    property color minBoxColor: ColorTheme.colors.roiMinimum
    property color maxBoxColor: ColorTheme.colors.roiMaximum

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
                return

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

        Rectangle
        {
            id: maxBox

            border.width: 2
            border.color: maxBoxColor
            color: ColorTheme.transparent(maxBoxColor, 0.3)

            x: F.absX(maxRect.x, figure)
            y: F.absY(maxRect.y, figure)
            width: F.absX(maxRect.width, figure)
            height: F.absY(maxRect.height, figure)
        }

        PointGrip
        {
            id: maxTopLeftGrip

            color: maxBoxColor
            maxX: F.absX(d.maxP2.x - minRect.width, figure)
            maxY: F.absY(d.maxP2.y - minRect.height, figure)

            onXChanged: dragging && (d.maxP1.x = F.relX(x, figure))
            onYChanged: dragging && (d.maxP1.y = F.relY(y, figure))

            Binding
            {
                target: maxTopLeftGrip
                property: "x"
                value: F.absX(d.maxP1.x, figure)
                when: !maxTopLeftGrip.dragging
            }

            Binding
            {
                target: maxTopLeftGrip
                property: "y"
                value: F.absY(d.maxP1.y, figure)
                when: !maxTopLeftGrip.dragging
            }
        }

        PointGrip
        {
            id: maxBottomRightGrip

            color: maxBoxColor
            minX: F.absX(d.maxP1.x + minRect.width, figure)
            minY: F.absY(d.maxP1.y + minRect.height, figure)
            maxX: figure.width
            maxY: figure.height

            onXChanged: dragging && (d.maxP2.x = F.relX(x, figure))
            onYChanged: dragging && (d.maxP2.y = F.relY(y, figure))

            Binding
            {
                target: maxBottomRightGrip
                property: "x"
                value: F.absX(d.maxP2.x, figure)
                when: !maxBottomRightGrip.dragging
            }

            Binding
            {
                target: maxBottomRightGrip
                property: "y"
                value: F.absY(d.maxP2.y, figure)
                when: !maxBottomRightGrip.dragging
            }
        }

        PointGrip
        {
            id: maxTopRightGrip

            color: maxBoxColor
            minX: F.absX(d.maxP1.x + minRect.width, figure)
            maxX: figure.width
            maxY: F.absY(d.maxP2.y - minRect.height, figure)

            onXChanged: dragging && (d.maxP2.x = F.relX(x, figure))
            onYChanged: dragging && (d.maxP1.y = F.relY(y, figure))

            Binding
            {
                target: maxTopRightGrip
                property: "x"
                value: F.absX(d.maxP2.x, figure)
                when: !maxTopRightGrip.dragging
            }

            Binding
            {
                target: maxTopRightGrip
                property: "y"
                value: F.absY(d.maxP1.y, figure)
                when: !maxTopRightGrip.dragging
            }
        }

        PointGrip
        {
            id: maxBottomLeftGrip

            color: maxBoxColor
            maxX: F.absX(d.maxP2.x - minRect.width, figure)
            minY: F.absY(d.maxP1.y + minRect.height, figure)
            maxY: figure.height

            onXChanged: dragging && (d.maxP1.x = F.relX(x, figure))
            onYChanged: dragging && (d.maxP2.y = F.relY(y, figure))

            Binding
            {
                target: maxBottomLeftGrip
                property: "x"
                value: F.absX(d.maxP1.x, figure)
                when: !maxBottomLeftGrip.dragging
            }

            Binding
            {
                target: maxBottomLeftGrip
                property: "y"
                value: F.absY(d.maxP2.y, figure)
                when: !maxBottomLeftGrip.dragging
            }
        }

        PointGrip
        {
            id: maxLeftGrip

            color: maxBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeHorCursor
            axis: Drag.XAxis
            maxX: F.absX(d.maxP2.x - minRect.width, figure)
            maxY: figure.height
            y: (maxTopLeftGrip.y + maxBottomLeftGrip.y) / 2

            onXChanged: dragging && (d.maxP1.x = F.relX(x, figure))

            Binding
            {
                target: maxLeftGrip
                property: "x"
                value: F.absX(d.maxP1.x, figure)
                when: !maxLeftGrip.dragging
            }
        }

        PointGrip
        {
            id: maxRightGrip

            color: maxBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeHorCursor
            axis: Drag.XAxis
            minX: F.absX(d.maxP1.x + minRect.width, figure)
            maxX: figure.width
            maxY: figure.height
            y: maxLeftGrip.y

            onXChanged: dragging && (d.maxP2.x = F.relX(x, figure))

            Binding
            {
                target: maxRightGrip
                property: "x"
                value: F.absX(d.maxP2.x, figure)
                when: !maxRightGrip.dragging
            }
        }

        PointGrip
        {
            id: maxTopGrip

            color: maxBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeVerCursor
            axis: Drag.YAxis
            maxX: figure.width
            maxY: F.absY(d.maxP2.y - minRect.height, figure)
            x: (maxTopLeftGrip.x + maxTopRightGrip.x) / 2

            onYChanged: dragging && (d.maxP1.y = F.relY(y, figure))

            Binding
            {
                target: maxTopGrip
                property: "y"
                value: F.absY(d.maxP1.y, figure)
                when: !maxTopGrip.dragging
            }
        }

        PointGrip
        {
            id: maxBottomGrip

            color: maxBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeVerCursor
            axis: Drag.YAxis
            maxX: figure.width
            minY: F.absY(d.maxP1.y + minRect.height, figure)
            maxY: figure.height
            x: maxTopGrip.x

            onYChanged: dragging && (d.maxP2.y = F.relY(y, figure))

            Binding
            {
                target: maxBottomGrip
                property: "y"
                value: F.absY(d.maxP2.y, figure)
                when: !maxBottomGrip.dragging
            }
        }
    }

    Item
    {
        id: minDraggable

        Rectangle
        {
            id: minBox

            border.width: 2
            border.color: minBoxColor
            color: ColorTheme.transparent(minBoxColor, 0.3)

            x: F.absX(minRect.x, figure)
            y: F.absY(minRect.y, figure)
            width: F.absX(minRect.width, figure)
            height: F.absY(minRect.height, figure)
        }

        PointGrip
        {
            id: minTopLeftGrip

            color: minBoxColor
            minX: Math.max(F.absX(d.minP2.x - maxRect.width, figure), 0)
            maxX: Math.min(F.absX(d.minP2.x + maxRect.width, figure), figure.width)
            minY: Math.max(F.absY(d.minP2.y - maxRect.height, figure), 0)
            maxY: Math.min(F.absY(d.minP2.y + maxRect.height, figure), figure.height)

            onXChanged: dragging && (d.minP1.x = F.relX(x, figure))
            onYChanged: dragging && (d.minP1.y = F.relY(y, figure))

            Binding
            {
                target: minTopLeftGrip
                property: "x"
                value: F.absX(d.minP1.x, figure)
                when: !minTopLeftGrip.dragging
            }

            Binding
            {
                target: minTopLeftGrip
                property: "y"
                value: F.absY(d.minP1.y, figure)
                when: !minTopLeftGrip.dragging
            }
        }

        PointGrip
        {
            id: minBottomRightGrip

            color: minBoxColor
            minX: Math.max(F.absX(d.minP1.x - maxRect.width, figure), 0)
            maxX: Math.min(F.absX(d.minP1.x + maxRect.width, figure), figure.width)
            minY: Math.max(F.absY(d.minP1.y - maxRect.height, figure), 0)
            maxY: Math.min(F.absY(d.minP1.y + maxRect.height, figure), figure.height)

            onXChanged: dragging && (d.minP2.x = F.relX(x, figure))
            onYChanged: dragging && (d.minP2.y = F.relY(y, figure))

            Binding
            {
                target: minBottomRightGrip
                property: "x"
                value: F.absX(d.minP2.x, figure)
                when: !minBottomRightGrip.dragging
            }

            Binding
            {
                target: minBottomRightGrip
                property: "y"
                value: F.absY(d.minP2.y, figure)
                when: !minBottomRightGrip.dragging
            }
        }

        PointGrip
        {
            id: minTopRightGrip

            color: minBoxColor
            minX: Math.max(F.absX(d.minP1.x - maxRect.width, figure), 0)
            maxX: Math.min(F.absX(d.minP1.x + maxRect.width, figure), figure.width)
            minY: Math.max(F.absY(d.minP2.y - maxRect.height, figure), 0)
            maxY: Math.min(F.absY(d.minP2.y + maxRect.height, figure), figure.height)

            onXChanged: dragging && (d.minP2.x = F.relX(x, figure))
            onYChanged: dragging && (d.minP1.y = F.relY(y, figure))

            Binding
            {
                target: minTopRightGrip
                property: "x"
                value: F.absX(d.minP2.x, figure)
                when: !minTopRightGrip.dragging
            }

            Binding
            {
                target: minTopRightGrip
                property: "y"
                value: F.absY(d.minP1.y, figure)
                when: !minTopRightGrip.dragging
            }
        }

        PointGrip
        {
            id: minBottomLeftGrip

            color: minBoxColor
            minX: Math.max(F.absX(d.minP2.x - maxRect.width, figure), 0)
            maxX: Math.min(F.absX(d.minP2.x + maxRect.width, figure), figure.width)
            minY: Math.max(F.absY(d.minP1.y - maxRect.height, figure), 0)
            maxY: Math.min(F.absY(d.minP1.y + maxRect.height, figure), figure.height)

            onXChanged: dragging && (d.minP1.x = F.relX(x, figure))
            onYChanged: dragging && (d.minP2.y = F.relY(y, figure))

            Binding
            {
                target: minBottomLeftGrip
                property: "x"
                value: F.absX(d.minP1.x, figure)
                when: !minBottomLeftGrip.dragging
            }

            Binding
            {
                target: minBottomLeftGrip
                property: "y"
                value: F.absY(d.minP2.y, figure)
                when: !minBottomLeftGrip.dragging
            }
        }

        PointGrip
        {
            id: minLeftGrip

            color: minBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeHorCursor
            axis: Drag.XAxis
            minX: Math.max(F.absX(d.minP2.x - maxRect.width, figure), 0)
            maxX: Math.min(F.absX(d.minP2.x + maxRect.width, figure), figure.width)
            y: (minTopLeftGrip.y + minBottomLeftGrip.y) / 2

            onXChanged: dragging && (d.minP1.x = F.relX(x, figure))

            Binding
            {
                target: minLeftGrip
                property: "x"
                value: F.absX(d.minP1.x, figure)
                when: !minLeftGrip.dragging
            }
        }

        PointGrip
        {
            id: minRightGrip

            color: minBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeHorCursor
            axis: Drag.XAxis
            minX: Math.max(F.absX(d.minP1.x - maxRect.width, figure), 0)
            maxX: Math.min(F.absX(d.minP1.x + maxRect.width, figure), figure.width)
            y: minLeftGrip.y

            onXChanged: dragging && (d.minP2.x = F.relX(x, figure))

            Binding
            {
                target: minRightGrip
                property: "x"
                value: F.absX(d.minP2.x, figure)
                when: !minRightGrip.dragging
            }
        }

        PointGrip
        {
            id: minTopGrip

            color: minBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeVerCursor
            axis: Drag.YAxis
            minY: Math.max(F.absY(d.minP2.y - maxRect.height, figure), 0)
            maxY: Math.min(F.absY(d.minP2.y + maxRect.height, figure), figure.height)
            x: (minTopLeftGrip.x + minTopRightGrip.x) / 2

            onYChanged: dragging && (d.minP1.y = F.relY(y, figure))

            Binding
            {
                target: minTopGrip
                property: "y"
                value: F.absY(d.minP1.y, figure)
                when: !minTopGrip.dragging
            }
        }

        PointGrip
        {
            id: minBottomGrip

            color: minBoxColor
            borderColor: "transparent"
            cursorShape: Qt.SizeVerCursor
            axis: Drag.YAxis
            minY: Math.max(F.absY(d.minP1.y - maxRect.height, figure), 0)
            maxY: Math.min(F.absY(d.minP1.y + maxRect.height, figure), figure.height)
            x: minTopGrip.x

            onYChanged: dragging && (d.minP2.y = F.relY(y, figure))

            Binding
            {
                target: minBottomGrip
                property: "y"
                value: F.absY(d.minP2.y, figure)
                when: !minBottomGrip.dragging
            }
        }
    }

    hint: d.dragging ? null : qsTr("Set minimum and maximum object sizes.")

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

    Object
    {
        id: d

        property point minP1
        property point minP2
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
