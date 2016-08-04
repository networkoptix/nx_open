import QtQuick 2.6
import Nx 1.0
import Nx.Controls 1.0

Item
{
    id: multipleCamerasLayout

    property CameraItem activeItem: null

    Grid
    {
        id: grid

        anchors.fill: parent
        columns: 2
        rows: 2
        spacing: 2

        Repeater
        {
            CameraItem
            {
                id: item

                clip: true
                width: (grid.width - grid.spacing * (grid.columns - 1)) / grid.columns
                height: (grid.height - grid.spacing * (grid.rows - 1)) / grid.rows
                useDummyText: false

                layoutX: index % grid.columns
                layoutY: Math.floor(index / grid.columns)

                MouseArea
                {
                    id: mouseArea
                    anchors.fill: parent
                    onClicked: multipleCamerasLayout.activeItem = item
                }

                MaterialEffect
                {
                    anchors.fill: parent
                    mouseArea: mouseArea
                    rippleSize: 160
                }
            }

            model: 4

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
}
