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

        contentWidth: gridLayout.implicitSize.width
        contentHeight: gridLayout.implicitSize.height
        implicitContentWidth: gridLayout.implicitSize.width
        implicitContentHeight: gridLayout.implicitSize.height

        unzoomToCenter: zoomedItem

        Positioners.Grid
        {
            id: gridLayout

            readonly property real cellAspectRatio: 16 / 9

            property size implicitSize: contentImplicitSize()

            width: flickableView.contentWidth
            height: flickableView.contentHeight

            cellSize: Qt.size(
                width / layoutModel.gridBoundingRect.width,
                width / layoutModel.gridBoundingRect.width / cellAspectRatio)

            Repeater
            {
                id: repeater

                model: LayoutModel
                {
                    id: layoutModel

                    property rect previousRect: Qt.rect(0, 0, 0, 0)

                    onGridBoundingRectChanged:
                    {
                        flickableView.zoomedItem = null

                        if (previousRect.width === 0 && previousRect.height === 0)
                        {
                            previousRect = gridBoundingRect
                            flickableView.fitInView(true)
                            return
                        }

                        var rect = gridBoundingRect
                        var cellSize = gridLayout.cellSize

                        var dx = rect.x - previousRect.x
                        var dy = rect.y - previousRect.y
                        var dw = rect.width - previousRect.width
                        var dh = rect.height - previousRect.height

                        var cx = flickableView.contentX + dx * cellSize.width
                        var cy = flickableView.contentY + dy * cellSize.height
                        var cw = flickableView.contentWidth + dw * cellSize.width
                        var ch = flickableView.contentHeight + dh * cellSize.height

                        previousRect = rect

                        flickableView.setContentGeometry(cx, cy, cw, ch)
                        gridLayout.implicitSize = gridLayout.contentImplicitSize()
                        flickableView.fitInView()
                    }
                }

                delegate: Item
                {
                    id: item

                    Positioners.Grid.geometry: model.geometry

                    MouseArea
                    {
                        anchors.fill: parent

                        onDoubleClicked:
                        {
                            if (flickableView.zoomedItem !== item)
                            {
                                flickableView.zoomedItem = item
                                return
                            }

                            if (item.width !== flickableView.width
                                && item.height !== flickableView.height)
                            {
                                flickableView.fitInView()
                            }
                            else
                            {
                                flickableView.zoomedItem = null
                            }
                        }
                    }

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

            function contentImplicitSize()
            {
                var gridAspectRatio = cellAspectRatio
                    * Math.max(1, layoutModel.gridBoundingRect.width)
                    / Math.max(1, layoutModel.gridBoundingRect.height)

                return flickableView.width / flickableView.height < gridAspectRatio
                    ? Qt.size(flickableView.width, flickableView.width / gridAspectRatio)
                    : Qt.size(flickableView.height * gridAspectRatio, flickableView.height)
            }
        }

        onDoubleClicked: fitInView()
    }
}
