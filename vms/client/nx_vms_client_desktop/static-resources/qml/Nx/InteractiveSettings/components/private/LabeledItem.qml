import QtQuick 2.0
import nx.vms.client.desktop 1.0

FocusScope
{
    id: labeledItem

    property string name: ""
    property string description: ""

    property Item contentItem: null

    property real spacing: 8

    // This property is used to calculate labels column width.
    readonly property alias implicitCaptionWidth: label.implicitWidth
    property alias captionWidth: label.width
    property alias caption: label.text

    implicitHeight: Math.max(label.implicitHeight, contentItem ? contentItem.implicitHeight : 0)
    implicitWidth: label.width + (contentItem ? contentItem.implicitWidth + spacing : 0)

    Label
    {
        id: label
        height: parent.height
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignRight
        wrapMode: Text.WordWrap

        GlobalToolTip.text: description
    }

    onContentItemChanged:
    {
        if (!contentItem)
            return

        contentItem.parent = labeledItem
        contentItem.focus = true
        contentItem.x = Qt.binding(
            function() { return label.width + spacing })
        contentItem.y = Qt.binding(
            function() { return (labeledItem.height - contentItem.height) / 2 })
        contentItem.width = Qt.binding(
            function() { return labeledItem.width - contentItem.x })
        contentItem.GlobalToolTip.text = Qt.binding(
            function() { return description })
    }
}
