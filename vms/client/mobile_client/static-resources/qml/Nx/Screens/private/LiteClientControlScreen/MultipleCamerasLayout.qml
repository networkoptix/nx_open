import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0
import Nx.Core 1.0

Item
{
    id: multipleCamerasLayout

    property CameraItem activeItem: null
    property int activeItemIndex: 0
    property LiteClientLayoutHelper layoutHelper: null

    readonly property alias gridWidth: grid.columns
    readonly property alias gridHeight: grid.rows

    Grid
    {
        id: grid

        anchors.fill: parent
        columns: 2
        rows: 2
        spacing: 2

        Repeater
        {
            id: repeater

            CameraItem
            {
                id: item

                clip: true
                width: (grid.width - grid.spacing * (grid.columns - 1)) / grid.columns
                height: (grid.height - grid.spacing * (grid.rows - 1)) / grid.rows
                useDummyText: false

                layoutHelper: multipleCamerasLayout.layoutHelper
                layoutX: index % grid.columns
                layoutY: Math.floor(index / grid.columns)

                MouseArea
                {
                    id: mouseArea
                    anchors.fill: parent
                    onClicked: item.activate()
                }

                MaterialEffect
                {
                    anchors.fill: parent
                    mouseArea: mouseArea
                    rippleSize: 160
                }

                function activate()
                {
                    multipleCamerasLayout.activeItem = item
                    multipleCamerasLayout.activeItemIndex = index
                }
            }

            model: grid.rows * grid.columns

            onCountChanged: activeItem = itemAt(0)
        }
    }

    Rectangle
    {
        parent: activeItem
        anchors.fill: parent
        border.color: ColorTheme.brand_main
        border.width: 2
        color: "transparent"
    }

    Connections
    {
        target: layoutHelper
        onLayoutChanged: resetLayout()
    }

    onLayoutHelperChanged: resetLayout()

    onActiveItemIndexChanged:
    {
        if (activeItemIndex >= repeater.count)
        {
            activeItemIndex = 0
            return
        }

        var item = repeater.itemAt(activeItemIndex)
        if (item)
            item.activate()
    }

    function itemAt(x, y)
    {
        var index = y * grid.columns + x
        return repeater.itemAt(index)
    }

    function resetLayout()
    {
        if (!layoutHelper)
            return

        for (var y = 0; y < gridHeight; ++y)
        {
            for (var x = 0; x < gridWidth; ++x)
            {
                var item = itemAt(x, y)
                if (item)
                    item.resourceId = layoutHelper.cameraIdOnCell(x, y)
            }
        }
    }
}
