// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import Nx.Core 1.0

import nx.vms.client.core 1.0

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

        onItemAdded: (index, item) =>
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
