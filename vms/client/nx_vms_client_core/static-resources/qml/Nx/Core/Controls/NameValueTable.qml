// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Controls
import Nx.Core
import Nx.Core.Controls

Item
{
    id: control

    /**
     * An array of objects with the following properties:
     *      ${nameRole}, default "name" - item's name
     *      ${valuesRole}, default "values" - item's values or a single value
     *      ${colorsRole}, default "colors" [optional] - item's colors or a single color,
     *          if present must have the same length as values
     */
    property var items: []

    property string nameRole: "name"
    property string valuesRole: "values"
    property string colorsRole: "colors"

    // If positive, only maxRowCount rows will be visible.
    property alias maxRowCount: grid.maxRowCount
    property int maximumLineCount: 2
    property alias rowSpacing: grid.rowSpacing
    property real colorBoxSize: 16

    property color nameColor: "gray"
    property color valueColor: "white"
    property int valueAlignment: Text.AlignLeft

    property font font
    property font nameFont: control.font
    property font valueFont: control.font

    onNameFontChanged: widthCalculator.updateColumnWidths()
    onValueFontChanged: widthCalculator.updateColumnWidths()

    // Default: 30% to label, 70% to value. This isn't strict proportion and uses only if there
    // is not enough space for labels or values.
    property real labelFraction: 0.3

    property bool interactive: false
    property Menu contextMenu
    readonly property alias hoveredRow: highlight.rowIndex
    readonly property var hoveredItem: hoveredRow >= 0 ? items[hoveredRow] : undefined

    property real tableLineHeight: CoreSettings.iniConfigValue("attributeTableLineHeightFactor")
    property real attributeTableSpacing: CoreSettings.iniConfigValue("attributeTableSpacing")

    property alias leftPadding: grid.leftPadding
    property alias rightPadding: grid.rightPadding
    property alias topPadding: grid.topPadding
    property alias bottomPadding: grid.bottomPadding

    implicitWidth: 400
    implicitHeight: grid.implicitHeight

    function forceLayout()
    {
        grid.forceLayout()
    }

    Rectangle
    {
        id: highlight

        property int rowIndex: -1

        color: ColorTheme.colors.dark12
        width: control.width
        visible: interactive && (rowIndex >= 0 || control.contextMenu?.opened)
    }

    Grid
    {
        id: grid

        objectName: "dataTable"

        readonly property var sourceData: control.items ?? []
        property int maxRowCount: 0
        readonly property var clippedData: control.maxRowCount > 0
            ? sourceData.slice(0, control.maxRowCount)
            : sourceData

        columns: 2
        leftPadding: control.interactive ? 8 : 0
        rightPadding: leftPadding
        width: control.width
        columnSpacing: 8
        rowSpacing: 0
        verticalItemAlignment: Grid.AlignVCenter
        flow: Grid.TopToBottom

        onClippedDataChanged:
        {
            if (control.contextMenu?.opened)
                control.contextMenu.close()

            labelsRepeater.model = clippedData
            valuesRepeater.model = clippedData
        }

        NameValueTableCalculator
        {
            id: widthCalculator
        }

        Repeater
        {
            id: labelsRepeater

            delegate: Text
            {
                id: cellLabel

                objectName: "cellLabel"

                readonly property int rowIndex: index

                color: control.nameColor
                font: control.nameFont

                text: modelData[control.nameRole] || " "
                textFormat: Text.StyledText

                maximumLineCount: control.maximumLineCount
                wrapMode: Text.Wrap
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignLeft

                lineHeight: control.tableLineHeight

                topPadding: Math.round(control.attributeTableSpacing / 2)
                bottomPadding: (control.attributeTableSpacing - topPadding)
            }
        }

        Item
        {
            width: 1 //< Shouldn't push the name column's width
            height: footer.height
            visible: footer.remainder > 0

            Text
            {
                id: footer

                readonly property int remainder:
                    grid.sourceData.length - grid.clippedData.length

                objectName: "footer"
                text: qsTr("+ %n more", "Numerus: placeholder for more items", remainder)
                width: grid.width
                color: nameColor
                lineHeight: tableLineHeight
                font: control.nameFont
                wrapMode: Text.Wrap
            }
        }

        Repeater
        {
            id: valuesRepeater

            delegate: ValuesText
            {
                id: combinedValueText

                objectName: "cellValues"
                spacing: 4
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: control.valueColor
                appendixColor: ColorTheme.darker(control.valueColor, 10)
                font: control.valueFont
                values: modelData[control.valuesRole] ?? []
                colorValues: modelData[control.colorsRole] ?? []
                maximumLineCount: control.maximumLineCount
                colorBoxSize: control.colorBoxSize
            }
        }
    }

    MouseArea
    {
        id: gridMouseArea

        enabled: control.interactive
        anchors.fill: grid
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        onPositionChanged: (mouse) =>
        {
            if (mouse.buttons !== Qt.NoButton)
                return

            const row = grid.childAt(grid.leftPadding, mouse.y)
            if (!row)
                return

            highlight.y = row.y
            highlight.height = row.height
            highlight.rowIndex = row.rowIndex

            const cell = grid.childAt(mouse.x, mouse.y)
            if (cell && cell.objectName === "cellValues")
            {
                toolTip.dataItem = cell
                toolTip.y = cell.y - toolTip.height
            }
            else
            {
                toolTip.hide()
            }
        }

        onExited:
        {
            highlight.rowIndex = -1
            toolTip.hide()
        }

        onClicked:
        {
            if (contextMenu)
                contextMenu.popup()
        }
    }

    ToolTip
    {
        id: toolTip

        property var dataItem: null
        readonly property var values: dataItem?.values ?? []
        readonly property var colors: dataItem?.colors ?? []

        parent: grid
        visible: !!dataItem
        popupType: Popup.Window

        contentItem: Row
        {
            spacing: 4

            Repeater
            {
                id: toolTipRepeater
                model: toolTip.values

                Row
                {
                    spacing: 4

                    Rectangle
                    {
                        readonly property bool relevant: !!toolTip.colors[index]
                        width: 16
                        height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        visible: relevant
                        color: toolTip.colors[index] ?? "transparent"
                        border.color: ColorTheme.transparent(ColorTheme.colors.light1, 0.1)
                        radius: 1
                    }

                    Text
                    {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                            + (index < toolTipRepeater.model.length - 1 && !toolTip.colors[index]
                                ? ","
                                : "")
                        lineHeight: control.tableLineHeight
                    }
                }
            }
        }
    }
}
