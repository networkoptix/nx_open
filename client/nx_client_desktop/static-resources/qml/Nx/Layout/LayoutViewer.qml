import QtQuick 2.6
import QtQuick.Layouts 1.3
import Qt.labs.templates 1.0
import Nx 1.0
import Nx.Utils 1.0
import Nx.Models 1.0
import Nx.Positioners 1.0 as Positioners

import "Items"

Control
{
    id: layoutViewer

    property var layout: null

    background: Rectangle { color: ColorTheme.windowBackground }

    contentItem: FlickableView
    {
        id: flickableView

        contentWidthPartToBeAlwaysVisible: 0.3
        contentHeightPartToBeAlwaysVisible: 0.3

        alignToCenterWhenUnzoomed: zoomedItem

        property real fitInViewSpacing: 0
        property real gridSpacing: zoomedItem ? 0 : fitInViewSpacing

        leftMargin: -gridSpacing
        rightMargin: -gridSpacing
        topMargin: -gridSpacing
        bottomMargin: -gridSpacing

        Positioners.Grid
        {
            id: gridLayout

            property real cellAspectRatio: layout ? layout.cellAspectRatio : 1
            Behavior on cellAspectRatio
            {
                NumberAnimation
                {
                    duration: 150

                    // NumberAnimation inside Behavior does not emit stopped() signal.
                    onRunningChanged:
                    {
                        if (running)
                            return

                        var contentAspectRatio = gridLayout.cellAspectRatio
                            * layoutModel.gridBoundingRect.width
                            / layoutModel.gridBoundingRect.height

                        flickableView.setContentGeometry(
                            flickableView.contentX,
                            flickableView.contentY,
                            flickableView.contentWidth,
                            flickableView.contentWidth / contentAspectRatio)
                        flickableView.fitInView()
                    }
                }
            }

            readonly property real cellSpacingFactor: layout ? layout.cellSpacing : 0
            readonly property real cellSpacing:
            {
                if (layoutModel.gridBoundingRect.width === 0
                    || layoutModel.gridBoundingRect.height === 0)
                {
                    return 0
                }

                var contentAspectRatio = gridLayout.cellAspectRatio
                    * layoutModel.gridBoundingRect.width
                    / layoutModel.gridBoundingRect.height

                var implicitContentWidth =
                    flickableView.implicitContentGeometry(contentAspectRatio).width

                var cellWidth = implicitContentWidth / layoutModel.gridBoundingRect.width

                return layout.cellSpacing * cellWidth
            }

            onCellSpacingChanged:
            {
                // Don't use binding for fitInViewSpacing!
                // We must guarantee it is assigned before fitInView call.
                flickableView.fitInViewSpacing = cellSpacing / 2
                flickableView.fitInView(true)
            }

            readonly property real unzoomThreshold: 10

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

                    layoutId: layout ? layout.id : Nx.uuid("")

                    property rect previousRect: Qt.rect(0, 0, 0, 0)

                    onGridBoundingRectChanged:
                    {
                        flickableView.zoomedItem = null

                        if (previousRect.width === 0 || previousRect.height === 0)
                        {
                            var contentAspectRatio = gridLayout.cellAspectRatio
                                * layoutModel.gridBoundingRect.width
                                / layoutModel.gridBoundingRect.height

                            previousRect = gridBoundingRect

                            if (gridBoundingRect.width === 0 || gridBoundingRect.height === 0)
                                return

                            var crect = flickableView.implicitContentGeometry(contentAspectRatio)
                            flickableView.setContentGeometry(
                                crect.x, crect.y, crect.width, crect.height)

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
                        flickableView.fitInView()
                    }
                }

                delegate: Item
                {
                    Positioners.Grid.geometry: model.geometry

                    ResourceItem
                    {
                        anchors.centerIn: parent
                        width: parent.width * (1 - gridLayout.cellSpacingFactor)
                        height: parent.height * (1 - gridLayout.cellSpacingFactor)
                        modelData: model
                    }
                }
            }
        }

        onDoubleClicked:
        {
            flickableView.zoomedItem = null
            fitInView()
        }
    }
}
