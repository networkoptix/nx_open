// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import nx.vms.client.desktop

import Nx
import Nx.Core
import Nx.Controls
import Nx.Debug
import Nx.Items
import Nx.Layout
import Nx.RightPanel

Control
{
    id: mainWindowRoot
    clip: true

    readonly property real panelDiscreteResizeEpsilon: 8
    property color resizeIndicatorColor: ColorTheme.transparent(ColorTheme.colors.dark10, 0.2)

    background: Rectangle { color: ColorTheme.window }

    contentItem: Item
    {
        LayoutView
        {
            id: layoutViewer

            z: -1
            anchors.left: resourcesPanel.right
            anchors.right: rightPanel.left
            anchors.top: titlePanel.bottom
            anchors.bottom: timelinePanel.top

            layout: workbench && workbench.currentLayout ? workbench.currentLayout.resource : null

            PerformanceText
            {
                anchors.top: parent.top
                anchors.right: parent.right
            }
        }

        // ----------------------------------------------------------------------------------------
        // Resources panel

        Resizer
        {
            id: resourcesPanelResizer

            edge: Qt.RightEdge
            target: resourcesPanel
            drag.minimumX: resourcesPanel.minimumWidth
            drag.maximumX: resourcesPanel.maximumWidth
        }

        Rectangle
        {
            id: resourcesPanelResizeIndicator

            width: resourcesPanelResizer.handleWidth
            color: resourcesPanelResizer.pressed ? resizeIndicatorColor : "transparent"

            anchors.left: resourcesPanel.right
            anchors.top: resourcesPanel.top
            anchors.bottom: resourcesPanel.bottom
        }

        SidePanel
        {
            id: resourcesPanel

            readonly property real minimumWidth: 200
            readonly property real maximumWidth: 640

            edge: Qt.LeftEdge
            openCloseDuration: 300
            width: 400

            opened: true

            anchors.top: titlePanel.bottom
            anchors.bottom: timelinePanel.top
            anchors.right: resourcesPanelResizer.dragging ? resourcesPanelResizer.left : undefined

            contentItem: ResourceBrowser
            {
                scene: layoutViewer
            }
        }

        // ----------------------------------------------------------------------------------------
        // Right panel

        Resizer
        {
            id: rightPanelResizer

            edge: Qt.LeftEdge
            target: rightPanel

            onDragPositionChanged:
            {
                const threshold = (rightPanel.narrowWidth + rightPanel.wideWidth) / 2
                const requestedWidth = rightPanel.x + rightPanel.width - pos

                if (requestedWidth > threshold + panelDiscreteResizeEpsilon)
                    rightPanel.width = rightPanel.wideWidth
                else if (requestedWidth < threshold - panelDiscreteResizeEpsilon)
                    rightPanel.width = rightPanel.narrowWidth
            }
        }

        Rectangle
        {
            id: rightPanelResizeIndicator

            width: rightPanelResizer.handleWidth
            color: rightPanelResizer.pressed ? resizeIndicatorColor : "transparent"

            anchors.right: rightPanel.left
            anchors.top: rightPanel.top
            anchors.bottom: rightPanel.bottom
        }

        SidePanel
        {
            id: rightPanel

            readonly property real narrowWidth: 280
            readonly property real wideWidth: 430

            edge: Qt.RightEdge
            openCloseDuration: 300
            width: narrowWidth
            opened: true

            anchors { top: titlePanel.bottom; bottom: timelinePanel.top }

            contentItem: RightPanel
            {
                id: rightPanelTabs
            }
        }

        // ----------------------------------------------------------------------------------------
        // Title panel

        SidePanel
        {
            id: titlePanel

            edge: Qt.TopEdge
            borderColor: "transparent"
            openCloseDuration: 100
            height: 24

            backgroundColor: ColorTheme.colors.dark6
        }

        // ----------------------------------------------------------------------------------------
        // Timeline panel

        Resizer
        {
            id: timelinePanelResizer

            edge: Qt.TopEdge
            target: timelinePanel

            drag.maximumY: timelinePanel.y + timelinePanel.height - timeline.implicitHeight
            drag.minimumY: drag.maximumY - timelinePanel.maxThumbnailHeight

            onDragPositionChanged:
            {
                const narrowHeight = timeline.implicitHeight
                const threshold = narrowHeight + timelinePanel.minThumbnailHeight / 2
                const requestedHeight = timelinePanel.y + timelinePanel.height - pos
                const minimumWideHeight = narrowHeight + timelinePanel.minThumbnailHeight

                if (requestedHeight > threshold + panelDiscreteResizeEpsilon)
                    timelinePanel.height = Math.max(requestedHeight, minimumWideHeight)
                else if (requestedHeight < threshold - panelDiscreteResizeEpsilon)
                    timelinePanel.height = timeline.height
            }
        }

        Rectangle
        {
            id: timelinePanelResizeIndicator

            height: timelinePanelResizer.handleWidth
            color: timelinePanelResizer.pressed ? resizeIndicatorColor : "transparent"

            anchors.left: timelinePanel.left
            anchors.right: timelinePanel.right
            anchors.bottom: timelinePanel.top
        }

        SidePanel
        {
            id: timelinePanel

            edge: Qt.BottomEdge
            openCloseDuration: 240
            backgroundColor: ColorTheme.colors.dark6
            height: timeline.implicitHeight

            readonly property real minThumbnailHeight: 48
            readonly property real maxThumbnailHeight: 196

            contentItem: ColumnLayout
            {
                spacing: 0

                Rectangle
                {
                    id: thumbnails

                    color: "black"
                    visible: timelinePanel.height > timeline.height

                    Layout.fillHeight: true
                    Layout.preferredWidth: parent.width

                    Rectangle
                    {
                        id: thumbnailsBorder

                        color: timelinePanel.borderColor
                        height: timelinePanel.borderWidth

                        anchors { left: parent.left; right: parent.right; top: parent.top }
                    }
                }

                Timeline
                {
                    id: timeline
                    Layout.preferredWidth: parent.width
                }
            }
        }
    }
}
