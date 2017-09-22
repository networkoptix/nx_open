import QtQuick 2.6
import QtQuick.Layouts 1.3
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Utils 1.0
import Nx.Models 1.0
import Nx.Positioners 1.0 as Positioners

Control
{
    property alias layoutId: layoutModel.layoutId

    background: Rectangle { color: ColorTheme.windowBackground }

    Positioners.Grid
    {
        id: gridLayout
        anchors.fill: parent

        cellSize: Qt.size(
            availableWidth / gridSize.width,
            availableWidth / gridSize.width / 16 * 9)

        Repeater
        {
            id: repeater

            model: LayoutModel
            {
                id: layoutModel
            }

            delegate: Item
            {
                width: 50
                height: 50

                Rectangle
                {
                    anchors.fill: parent
                    anchors.margins: 2
                    color: "#70ff0000"
                }

                Text
                {
                    anchors.centerIn: parent
                    text: model.name
                }

                Positioners.Grid.geometry: model.geometry
            }
        }
    }
}
