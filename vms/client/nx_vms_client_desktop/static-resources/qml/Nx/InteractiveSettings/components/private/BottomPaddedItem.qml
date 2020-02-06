import QtQuick 2.6
import QtQuick.Controls 2.4

Item
{
    property bool isGroup: false
    property alias paddedItem: control.contentItem
    property real conditionalBottomPadding: 8

    implicitWidth: control.implicitWidth
    implicitHeight: control.implicitHeight

    Control
    {
        id: control

        padding: 0
        bottomPadding: needsBottomPadding ? conditionalBottomPadding : 0
        anchors.fill: parent
    }

    readonly property bool needsBottomPadding:
    {
        if (isGroup)
            return true

        if (!Positioner || !parent || Positioner.isLastItem)
            return false

        var next = parent.children[Positioner.index + 1]
        return next && next.isGroup || false
    }
}
