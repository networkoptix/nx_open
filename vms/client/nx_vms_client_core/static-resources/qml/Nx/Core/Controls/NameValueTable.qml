// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core

Item
{
    id: control

    property var items: []

    // If positive, only maxRowCount rows will be visible.
    property int maxRowCount: 0
    property alias rowSpacing: grid.rowSpacing

    property color nameColor: "gray"
    property color valueColor: "white"
    property int valueAlignment: Text.AlignLeft

    property string textRoleName: "text"
    property string valuesRoleName: "values"

    // If color is specified, a color rectangle icon is added to the line.
    property string colorsRoleName: "colors"

    property font font
    property font nameFont: control.font
    property font valueFont: control.font

    property real labelFraction: 0.4 //< Default: 40% to label, 60% to value.

    property bool copyable: false
    property string copyIconPath
    readonly property int kCopyWidth: 34
    readonly property int kHighlightLeftPadding: 8

    signal searchRequested(int row)

    property real tableLineHeight: CoreSettings.iniConfigValue("attributeTableLineHeightFactor")
    property real attributeTableSpacing: CoreSettings.iniConfigValue("attributeTableSpacing")

    implicitWidth: 400
    implicitHeight: grid.implicitHeight

    function forceLayout()
    {
        grid.forceLayout()
    }

    function rowsCount()
    {
        return control.items.length / 2;
    }

    onItemsChanged:
    {
        repeater.model = items
        grid.updateColumnWidths()
    }

    Rectangle
    {
        id: highlight

        property int rowIndex: -1

        color: ColorTheme.colors.dark12
        width: kHighlightLeftPadding + grid.width + kCopyWidth
        visible: copyable && (gridMouseArea.containsMouse || contextMenu.opened)

        Item
        {
            anchors.right: parent.right

            width: kCopyWidth
            height: parent.height
            visible: !contextMenu.opened

            ColoredImage
            {
                id: icon

                width: 20
                height: 20

                anchors.centerIn: parent

                property color mainColor: ColorTheme.windowText
                property color hoveredColor: ColorTheme.lighter(mainColor, 2)
                property color pressedColor: mainColor

                property bool hovered: gridMouseArea.containsMouse
                    && gridMouseArea.mouseX > gridMouseArea.width - kCopyWidth

                property bool pressed: gridMouseArea.containsPress
                    && gridMouseArea.mouseX > gridMouseArea.width - kCopyWidth

                primaryColor: pressed ? pressedColor : (hovered ? hoveredColor : mainColor)
                sourcePath: copyable
                    ? copyIconPath
                    : ""
                sourceSize: Qt.size(width, height)
            }
        }
    }

    Grid
    {
        id: grid

        columns: 2
        x: copyable ? kHighlightLeftPadding : 0
        width: control.width - (copyable ? (kCopyWidth + kHighlightLeftPadding) : 0)
        columnSpacing: 8
        rowSpacing: 0
        verticalItemAlignment: Grid.AlignVCenter

        property var rowLookup: []

        onWidthChanged:
        {
            updateColumnWidths()
        }

        Component.onCompleted:
            updateColumnWidths()

        onPositioningComplete:
            updateRowLookup()

        component Values: Row
        {
            id: cellValuesRow

            property var text
            property var values
            property var colors
            property int lastVisibleIndex: 0

            spacing: 4

            Repeater
            {
                id: rowRepeater

                anchors.verticalCenter: parent.verticalCenter
                objectName: "valueRowRepeater"
                model: values

                Row
                {
                    id: valueItem

                    objectName: "valueItem"
                    spacing: 4
                    visible: index <= lastVisibleIndex

                    Rectangle
                    {
                        objectName: "colorRectangle"
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !!colors[index]
                        color: colors[index] ?? "transparent"
                        border.color: ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
                        radius: 1
                    }

                    Text
                    {
                        objectName: "valueText"
                        anchors.verticalCenter: parent.verticalCenter
                        color: control.valueColor
                        font: control.valueFont
                        text: modelData + (index < lastVisibleIndex && !colors[index] ? "," : "")
                        lineHeight: control.tableLineHeight
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                    }
                }
            }

            Text
            {
                id: appendix

                anchors.verticalCenter: parent.verticalCenter
                visible: lastVisibleIndex < rowRepeater.count - 1
                text: `+ ${rowRepeater.count - lastVisibleIndex - 1}`
                color: ColorTheme.darker(control.valueColor, 10)
                font: control.valueFont
            }

            onWidthChanged:
            {
                lastVisibleIndex = rowRepeater.count - 1
                var sumWidth = 0
                for (let i = 0; i < rowRepeater.count; ++i)
                {
                    const item = rowRepeater.itemAt(i)
                    if (sumWidth > 0)
                        sumWidth += spacing
                    sumWidth += item.implicitWidth
                    if (sumWidth > width)
                    {
                        lastVisibleIndex = i - 1
                        break
                    }
                }
                // At least one elided value we should show.
                lastVisibleIndex = Math.max(0, lastVisibleIndex)
            }
        }

        Repeater
        {
            id: repeater

            objectName: "dataTable"
            delegate: Component
            {
                Row
                {
                    readonly property bool isLabel: (index % 2) === 0
                    readonly property int rowIndex: Math.floor(index / 2)

                    spacing: 4

                    Text
                    {
                        id: cellLabel

                        objectName: "cellLabel"
                        readonly property bool allowed: isLabel
                            &&  (control.maxRowCount === 0 || rowIndex < control.maxRowCount)
                        visible: allowed
                        color: control.nameColor
                        font: control.nameFont

                        text: modelData[textRoleName] ?? modelData
                        textFormat: Text.StyledText

                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        horizontalAlignment: Text.AlignLeft

                        lineHeight: control.tableLineHeight

                        // Copy icon requires more vertical space.
                        topPadding: copyable
                            ? 4
                            : Math.round(control.attributeTableSpacing / 2)
                        bottomPadding: copyable
                            ? 4
                            : (control.attributeTableSpacing - topPadding)
                    }

                    Values
                    {
                        id: cellValues

                        objectName: "cellValues"
                        readonly property bool allowed: !isLabel
                            && (control.maxRowCount === 0 || rowIndex < control.maxRowCount)
                        visible: allowed
                        colors: modelData[colorsRoleName]
                        values: modelData[valuesRoleName]
                        text: modelData[textRoleName] ?? modelData
                    }
                }
            }
        }

        Text
        {
            id: footer

            objectName: "footer"
            visible: control.maxRowCount && rowsCount() > control.maxRowCount
            text: qsTr("+ %n more", "", rowsCount() - control.maxRowCount)
            color: nameColor
            lineHeight: tableLineHeight
            font: control.nameFont
            wrapMode: Text.Wrap
        }

        function updateColumnWidths()
        {
            function maximumWidth(maxWidth, item)
            {
                return Math.max(maxWidth, item.implicitWidth)
            }

            const labels = findAllSubitems(this,
                function(item)
                {
                    return item.objectName === "cellLabel" && item.allowed
                })
            const implicitLabelWidth = Array.prototype.reduce.call(labels, maximumWidth, 0)

            const values = findAllSubitems(this,
                function(item)
                {
                    return item.objectName === "cellValues" && item.allowed
                })
            const implicitValueWidth = Array.prototype.reduce.call(values, maximumWidth, 0)

            const availableWidth = grid.width - grid.columnSpacing

            const preferredLabelWidth = availableWidth * control.labelFraction
            const preferredValueWidth = availableWidth - preferredLabelWidth

            let labelWidth = 0
            const availableLabelWidth = availableWidth - implicitValueWidth

            // If value is shorter than its preferred space...
            if (implicitValueWidth < preferredValueWidth)
            {
                // Label may occupy not less than its preferred space
                // and no more than its available space.
                labelWidth = Math.max(preferredLabelWidth,
                    Math.min(implicitLabelWidth, availableLabelWidth))
            }
            else // If value is longer than its preferred space...
            {
                // Label may occupy not less than its available space
                // and no more than its preferred space.
                labelWidth = Math.max(availableLabelWidth,
                    Math.min(implicitLabelWidth, preferredLabelWidth))
            }
            const valueWidth = availableWidth - labelWidth
            for (var i = 0; i < labels.length; ++i)
                labels[i].width = labelWidth
            for (var i = 0; i < values.length; ++i)
                values[i].width = valueWidth
            updateRowLookup()
        }

        Timer
        {
            id: rowLookupTimer
            interval: 100
            repeat: false
            onTriggered:
                grid.buildRowLookup()
        }

        function updateRowLookup()
        {
            if (copyable)
                rowLookupTimer.start()
        }

        function buildRowLookup()
        {
            // Build an array for row lookup by its vertical position.
            // All rows are sorted by Y, ascending.

            // For the last item in the array, compute its height using Y coordinate
            // of the provided next item.
            const computeHeightOfLastItem =
                (array, nextItem) =>
                {
                    if (array.length <= 0)
                        return

                    let prevItem = array[array.length - 1]
                    prevItem.height = nextItem.y - prevItem.y
                }

            // Convert Item's children array into JS Array.
            let allChildren = []
            for (let i = 0; i < children.length; ++i)
                allChildren.push(children[i])

            grid.rowLookup = allChildren.reduce(
                (result, child, index, array) =>
                {
                    if (child.isLabel)
                    {
                        computeHeightOfLastItem(result, child)

                        result.push({
                            y: child.y,
                            height: 0,
                            index: index / 2})
                    }
                    return result
                }, [])

            computeHeightOfLastItem(grid.rowLookup, {y: grid.height})
        }

        function getRowAtY(y)
        {
            // Perform a binary search to find a row containing specified Y coordinate.

            let lo = 0
            let hi = grid.rowLookup.length

            while (lo < hi)
            {
                const m = lo + ((hi - lo) >> 1)
                if (grid.rowLookup[m].y <= y)
                    lo = m + 1
                else
                    hi = m
            }

            return grid.rowLookup[lo - 1]
        }
    }

    MouseArea
    {
        id: gridMouseArea

        enabled: control.copyable
        anchors.fill: control
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPositionChanged: (mouse) =>
        {
            const row = grid.getRowAtY(mouse.y)
            if (!row)
                return

            highlight.y = row.y
            highlight.height = row.height
            highlight.rowIndex = row.index
        }

        onClicked: (mouse) =>
        {
            if (mouse.x <= gridMouseArea.width - kCopyWidth)
            {
                if (mouse.button === Qt.RightButton)
                    contextMenu.popup()
                return
            }

            if (highlight.rowIndex >= 0)
                control.searchRequested(highlight.rowIndex)
        }

        Menu
        {
            id: contextMenu

            property int rowIndex: -1

            onAboutToShow: rowIndex = highlight.rowIndex
            onAboutToHide: rowIndex = -1

            MenuItem
            {
                text: qsTr("Copy")
                onTriggered:
                {
                    NxGlobals.copyToClipboard(items[contextMenu.rowIndex * 2 + 1].text)
                }
            }

            MenuItem
            {
                text: qsTr("Filter by")
                onTriggered:
                {
                    if (contextMenu.rowIndex >= 0)
                        control.searchRequested(contextMenu.rowIndex)
                }
            }
        }
    }

    function findAllSubitems(root, filter)
    {
        var result = []
        findSubitemsRecursive(root, filter, result)
        return result
    }

    function findSubitemsRecursive(item, filter, result)
    {
        if (item === null || item === undefined)
            return

        if (filter(item))
            result.push(item)

        if (item.children)
        {
            for (var i = 0; i < item.children.length; ++i)
                findSubitemsRecursive(item.children[i], filter, result);
        }
    }
}
