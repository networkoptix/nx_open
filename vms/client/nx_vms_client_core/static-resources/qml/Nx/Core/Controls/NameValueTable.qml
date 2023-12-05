// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Core.Controls

Item
{
    id: control

    property var items: []
    property var rawItems: []

    property alias rowSpacing: grid.rowSpacing

    property color nameColor: "gray"
    property color valueColor: "white"
    property int valueAlignment: Text.AlignLeft

    property string textRoleName: "text"

    // If color is specified, a color rectangle icon is added to the line.
    property string colorRoleName: "color"

    property font font
    property font nameFont: control.font
    property font valueFont: control.font

    property real labelFraction: 0.4 //< Default: 40% to label, 60% to value.

    property bool copyable: false
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
        visible: copyable && gridMouseArea.containsMouse

        Item
        {
            anchors.right: parent.right

            width: kCopyWidth
            height: parent.height

            IconImage
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

                color: pressed ? pressedColor : (hovered ? hoveredColor : mainColor)
                 source: control.copyable
                    ? "image://svg/skin/advanced_search_dialog/tosearch.svg"
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

        property var rowLookup: []

        onWidthChanged:
            updateColumnWidths()

        Component.onCompleted:
            updateColumnWidths()

        onPositioningComplete:
            updateRowLookup()

        Repeater
        {
            id: repeater

            delegate: Component
            {
                Text
                {
                    id: textControl

                    readonly property bool isLabel: (index % 2) === 0

                    color: isLabel ? control.nameColor : control.valueColor
                    font: isLabel ? control.nameFont : control.valueFont

                    text: modelData[textRoleName] ?? modelData
                    textFormat: Text.StyledText

                    maximumLineCount: 2
                    wrapMode: Text.Wrap
                    elide: Text.ElideRight
                    horizontalAlignment: isLabel
                        ? Text.AlignLeft
                        : control.valueAlignment

                    lineHeight: control.tableLineHeight

                    // Copy icon requires more vertical space.
                    topPadding: copyable
                        ? 4
                        : Math.round(control.attributeTableSpacing / 2)
                    bottomPadding: copyable
                        ? 4
                        : (control.attributeTableSpacing - topPadding)
                    leftPadding: color.visible ? color.width + 4 : 0

                    Rectangle
                    {
                        id: color

                        property var modelColor: modelData[colorRoleName] || null

                        anchors.verticalCenter: parent.verticalCenter

                        width: 16
                        height: 16
                        visible: !!modelColor
                        color: modelColor ?? "transparent"
                        border.color: ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
                        radius: 1
                    }
                }
            }
        }

        function updateColumnWidths()
        {
            let implicitLabelWidth = 0
            let implicitValueWidth = 0

            for (let i = 0; i < children.length; ++i)
            {
                const child = children[i]
                if (!(child instanceof Text))
                    continue;

                if (child.isLabel)
                    implicitLabelWidth = Math.max(implicitLabelWidth, child.implicitWidth)
                else
                    implicitValueWidth = Math.max(implicitValueWidth, child.implicitWidth)
            }

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

            for (let i = 0; i < children.length; ++i)
            {
                const child = children[i]
                if (child instanceof Text)
                    child.width = child.isLabel ? labelWidth : valueWidth
            }

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

            grid.rowLookup = allChildren.filter(child => child instanceof Text)
                .reduce((result, child, index, array) =>
                    {
                        if (index % 2 === 0)
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
                return

            if (highlight.rowIndex >= 0)
                control.searchRequested(highlight.rowIndex)
        }
    }
}
