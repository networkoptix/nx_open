// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Controls
import Nx.RightPanel

import nx.vms.client.core
import nx.vms.client.desktop

import "private"

Item
{
    id: searchPanel

    property alias type: eventModel.type
    property alias brief: ribbon.brief
    property alias active: eventModel.active
    property alias placeholder: ribbon.placeholder
    property alias isConstrained: eventModel.isConstrained
    property bool limitToCurrentCamera: false

    property alias showInformation: counterBlock.showInformation
    property alias showThumbnails: counterBlock.showThumbnails

    // Whether "showInformation" and "showThumbnails" can be toggled.
    property alias showInformationButton: counterBlock.showInformationButton
    property alias showThumbnailsButton: counterBlock.showThumbnailsButton

    readonly property alias model: eventModel
    readonly property alias filtersColumn: header.filtersColumn

    property alias defaultCameraSelection: header.defaultCameraSelection

    signal filtersReset()

    EventRibbon
    {
        id: ribbon

        objectName: searchPanel.objectName + ".EventRibbon"

        tileController.showInformation: searchPanel.showInformation
        tileController.showThumbnails: searchPanel.showThumbnails

        width: searchPanel.width
        height: searchPanel.height - y

        readonly property real headerWidth: searchPanel.width - ScrollBar.vertical.width
        readonly property real headerHeight: headerItem ? headerItem.height : 0

        model: RightPanelModel
        {
            id: eventModel
            context: windowContext
            previewsEnabled: searchPanel.showThumbnails
        }

        CounterBlock
        {
            id: counterBlock

            width: ribbon.headerWidth
            y: Math.max(0, ribbon.originY - ribbon.contentY + ribbon.headerHeight - height)
            leftInset: 1

            displayedItemsText: eventModel.itemCountText
        }

        Column
        {
            id: headerColumn

            // Re-parent into ribbon viewport.
            parent: ribbon.headerItem

            width: ribbon.headerWidth
            bottomPadding: counterBlock.height

            SearchPanelHeader
            {
                id: header

                model: eventModel
                width: ribbon.headerWidth
                limitToCurrentCamera: searchPanel.limitToCurrentCamera

                onFiltersReset:
                    searchPanel.filtersReset()
            }

            Rectangle
            {
                id: separator

                color: ColorTheme.colors.dark6
                height: 1
                width: ribbon.headerWidth
            }
        }
    }
}
