import QtQuick 2.6

import nx.vms.client.desktop 1.0

BottomPaddedItem
{
    id: labeledItem

    property string name: ""
    property string description: ""

    property Item contentItem: null
    property Item toolTipTarget: contentItem

    property real spacing: 8

    property alias labelWidth: label.width
    property string caption

    width: parent.width

    readonly property bool isBaselineAligned: label.lineCount === 1
        && contentItem && contentItem.baselineOffset > 0

    paddedItem: FocusScope
    {
        id: focusScope

        implicitWidth: label.width + (contentItem ? contentItem.implicitWidth + spacing : 0)

        implicitHeight: isBaselineAligned
            ? Math.max(label.y + label.height, contentItem.y + contentItem.height)
            : Math.max(Math.min(label.height, 28), contentItem ? contentItem.height : 0)

        Label
        {
            id: label

            y: isBaselineAligned ? Math.max(contentItem.baselineOffset - baselineOffset, 0) : 0
            text: caption || " " //< Always non-empty to ensure fixed line height & baseline offset.
            horizontalAlignment: Text.AlignRight
            maximumLineCount: 2
            wrapMode: Text.WordWrap
            elide: Text.ElideRight

            contextHintText: description
            GlobalToolTip.text: truncated ? text : null
        }
    }

    onContentItemChanged:
    {
        if (!contentItem)
            return

        contentItem.parent = focusScope
        contentItem.focus = true

        contentItem.width = Qt.binding(
            function() { return labeledItem.width - contentItem.x })

        contentItem.x = Qt.binding(
            function() { return label.width + spacing })

        contentItem.y = Qt.binding(
            function()
            {
                return labeledItem.isBaselineAlignment
                    ? label.y + label.baselineOffset - contentItem.baselineOffset
                    : 0
            })
    }

    onToolTipTargetChanged:
    {
        if (!toolTipTarget)
            return

        toolTipTarget.GlobalToolTip.text = Qt.binding(
            function()
            {
                return labeledItem.hasOwnProperty("defaultTooltip")
                    ? "Default value: " + labeledItem.defaultTooltip
                    : ""
            })
    }
}
