import QtQuick 2.0
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

    implicitHeight: Math.max(label.y + label.implicitHeight, contentItem ? contentItem.implicitHeight : 0)
    implicitWidth: label.width + (contentItem ? contentItem.implicitWidth + spacing : 0)

    width: parent.width

    Label
    {
        id: label

        y: 4
        height: parent.height
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
