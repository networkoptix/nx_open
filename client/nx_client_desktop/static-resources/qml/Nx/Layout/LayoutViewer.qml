import QtQuick 2.6
import QtQuick.Layouts 1.3
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Utils 1.0
import Nx.Models 1.0
import Nx.Positioners 1.0 as Positioners

Control
{
    id: layoutViewer

    property alias layoutId: layoutModel.layoutId

    background: Rectangle { color: ColorTheme.windowBackground }

    contentItem: FlickableView
    {
        id: flickableView

        contentWidthPartToBeAlwaysVisible: 0.3
        contentHeightPartToBeAlwaysVisible: 0.3

        contentWidth: gridLayout.implicitWidth
        contentHeight: gridLayout.implicitHeight
        implicitContentWidth: gridLayout.implicitWidth
        implicitContentHeight: gridLayout.implicitHeight

        Positioners.Grid
        {
            id: gridLayout

            readonly property real cellAspectRatio: 16 / 9

            readonly property size implicitSize:
            {
                var gridAspectRatio = cellAspectRatio
                    * Math.max(1, gridSize.width) / Math.max(1, gridSize.height)

                return flickableView.width / flickableView.height < gridAspectRatio
                    ? Qt.size(flickableView.width, flickableView.width / gridAspectRatio)
                    : Qt.size(flickableView.height * gridAspectRatio, flickableView.height)
            }

            implicitWidth: implicitSize.width
            implicitHeight: implicitSize.height

            width: flickableView.contentWidth
            height: flickableView.contentHeight

            cellSize: Qt.size(
                width / gridSize.width,
                width / gridSize.width / 16 * 9)

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

                    Positioners.Grid.geometry: model.geometry

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
                }
            }
        }

        onDoubleClicked: fitInView()
    }
}
