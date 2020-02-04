import QtQuick 2.6

import nx.vms.client.desktop 1.0

FocusScope
{
    id: labeledItem

    property string name: ""
    property string description: ""

    property Item contentItem: null

    property real spacing: 8
    property alias labelY: label.y

    readonly property alias implicitLabelWidth: label.implicitWidth
    property alias labelWidth: label.width
    property alias caption: label.text

    implicitWidth: label.width + (contentItem ? contentItem.implicitWidth + spacing : 0)

    implicitHeight: isBaselineAligned
        ? Math.max(label.y + label.height, contentItem.y + contentItem.height)
        : Math.max(Math.min(label.height, 28), contentItem ? contentItem.height : 0)

    width: parent.width

    readonly property bool isBaselineAligned: label.lineCount === 1
        && contentItem && contentItem.baselineOffset > 0

    Label
    {
        id: label

        y: isBaselineAligned ? Math.max(contentItem.baselineOffset - baselineOffset, 0) : 0
        horizontalAlignment: Text.AlignRight
        verticalAlignment: Text.AlignVCenter
        height: Math.max(implicitHeight, isBaselineAligned ? 0 : 28)
        topPadding: lineCount === 1 && !isBaselineAligned ? 1 : 0 //< Fine tuning.
        maximumLineCount: 2
        wrapMode: Text.WordWrap
        elide: Text.ElideRight

        GlobalToolTip.text: description || (truncated ? text : null)
    }

    onContentItemChanged:
    {
        if (!contentItem)
            return

        contentItem.parent = labeledItem
        contentItem.focus = true

        contentItem.GlobalToolTip.text = Qt.binding(
            function() { return description })

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
}
