import QtQuick 2.6
import Nx.Media 1.0
import Nx.Core 1.0

Item
{
    property MediaResourceHelper resourceHelper: null
    property alias delegate: repeaterItem.delegate

    readonly property Repeater repeater: repeaterItem

    // Implementation.

    readonly property size layoutSize: resourceHelper ? resourceHelper.layoutSize : Qt.size(1, 1)
    readonly property real cellWidth: width / layoutSize.width
    readonly property real cellHeight: height / layoutSize.height

    Repeater
    {
        id: repeaterItem
        model: resourceHelper && resourceHelper.channelCount

        onItemAdded:
        {
            item.width = Qt.binding(
                function() { return cellWidth })
            item.height = Qt.binding(
                function() { return cellHeight })
            item.x = Qt.binding(
                function() { return resourceHelper.channelPosition(index).x * cellWidth })
            item.y = Qt.binding(
                function() { return resourceHelper.channelPosition(index).y * cellHeight })
        }
    }
}
