// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14
import QtQuick.Window 2.14
import QtQml 2.14

import Nx 1.0
import Nx.Core 1.0
import Nx.Controls 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

import "private"

GridView
{
    id: view

    property bool brief: false //< Only resource list and preview.

    property alias placeholder: controller.placeholder
    property alias tileController: controller.tileController
    property alias standardTileInteraction: controller.standardTileInteraction

    property real columnWidth: 240
    property real columnSpacing: 2
    property real rowHeight: d.automaticRowHeight
    property real rowSpacing: 2

    property real scrollStepSize: 20

    property var hoveredItem: null

    cellWidth: columnWidth + columnSpacing
    cellHeight: rowHeight + rowSpacing

    topMargin: 4
    leftMargin: 8
    rightMargin: 8 + scrollBar.width - columnSpacing
    bottomMargin: 8 - rowSpacing

    boundsBehavior: Flickable.StopAtBounds
    interactive: false
    clip: true

    ScrollBar.vertical: ScrollBar
    {
        id: scrollBar
        stepSize: view.scrollStepSize / view.contentHeight
    }

    Rectangle
    {
        id: scrollbarPlaceholder

        parent: scrollBar.parent
        anchors.fill: scrollBar
        color: ColorTheme.colors.dark5
        visible: view.visibleArea.heightRatio >= 1.0
    }

    delegate: TileLoader
    {
        id: loader

        width: view.cellWidth - columnSpacing
        height: view.cellHeight - rowSpacing
        tileController: controller.tileController

        Connections
        {
            target: loader.item

            function onHoveredChanged()
            {
                if (target.hovered)
                {
                    view.hoveredItem = loader
                    return
                }

                if (view.hoveredItem === loader)
                    view.hoveredItem = null
            }
        }
    }

    objectName: "EventGrid"

    LoggingCategory
    {
        id: loggingCategory
        name: "Nx.RightPanel.EventGrid"
    }

    ModelViewController
    {
        id: controller

        view: view
        loggingCategory: loggingCategory
    }

    header: Component { Column { width: parent.width }}
    footer: Component { Column { width: parent.width }}

    MouseArea
    {
        id: mouseArea

        acceptedButtons: Qt.NoButton
        anchors.fill: parent
        hoverEnabled: false
        z: -1

        AdaptiveMouseWheelTransmission { id: gearbox }

        onWheel: (wheel) =>
        {
            // TODO: imlement pixel scrolling for high precision touchpads.
            scrollBar.scrollBySteps(gearbox.transform(-wheel.angleDelta.y / 120.0))
        }
    }

    NxObject
    {
        id: d

        property real automaticRowHeight: 0
        property real maxImplicitRowHeight: 0

        function resetAutomaticRowHeight()
        {
            d.maxImplicitRowHeight = 0
            automaticRowHeightAnimation.enabled = false
        }

        Connections
        {
            target: view.Window.window

            function onAfterAnimating()
            {
                const implicitRowHeight = Array.prototype.reduce.call(view.contentItem.children,
                    function(maximumHeight, item)
                    {
                        return item instanceof TileLoader
                            ? Math.max(maximumHeight, item.implicitHeight)
                            : maximumHeight
                    },
                    /*initial value*/ 0)

                const kDefaultRowHeight = 400
                d.maxImplicitRowHeight = Math.max(d.maxImplicitRowHeight, implicitRowHeight)
                d.automaticRowHeight = d.maxImplicitRowHeight || kDefaultRowHeight
                automaticRowHeightAnimation.enabled = d.maxImplicitRowHeight > 0
            }
        }

        Connections
        {
            target: controller.tileController

            function onShowThumbnailsChanged() { d.resetAutomaticRowHeight() }
            function onShowInformationChanged() { d.resetAutomaticRowHeight() }
        }

        Behavior on automaticRowHeight
        {
            id: automaticRowHeightAnimation

            enabled: false
            animation: NumberAnimation { duration: 250 }
        }
    }

    onCountChanged:
    {
        if (count === 0)
            d.resetAutomaticRowHeight()
    }

    onCellWidthChanged:
        d.resetAutomaticRowHeight()
}
