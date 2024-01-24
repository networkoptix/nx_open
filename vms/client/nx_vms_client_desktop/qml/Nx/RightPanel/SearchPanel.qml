// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx
import Nx.Core
import Nx.Controls
import Nx.RightPanel

import nx.vms.client.core
import nx.vms.client.desktop

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

    property int defaultCameraSelection: limitToCurrentCamera
        ? RightPanel.CameraSelection.current
        : RightPanel.CameraSelection.all

    EventRibbon
    {
        id: ribbon

        objectName: searchPanel.objectName + ".EventRibbon"

        tileController.showInformation: searchPanel.showInformation
        tileController.showThumbnails: searchPanel.showThumbnails

        width: searchPanel.width
        height: searchPanel.height - y

        model: RightPanelModel
        {
            id: eventModel
            context: windowContext
            previewsEnabled: searchPanel.showThumbnails
        }

        Binding
        {
            target: eventModel.commonSetup
            property: "cameraSelection"
            value: searchPanel.defaultCameraSelection
        }

        CounterBlock
        {
            id: counterBlock

            width: searchPanel.width - ribbon.ScrollBar.vertical.width
            y: Math.max(0, ribbon.originY - ribbon.contentY + ribbon.headerItem.height - height)

            displayedItemsText: eventModel.itemCountText
        }

        Column
        {
            id: headerColumn

            // Re-parent into ribbon viewport.
            parent: ribbon.headerItem

            width: searchPanel.width
            bottomPadding: counterBlock.height

            SearchPanelHeader
            {
                id: header

                model: eventModel
                width: searchPanel.width
                limitToCurrentCamera: searchPanel.limitToCurrentCamera
            }

            Rectangle
            {
                id: separator

                color: ColorTheme.colors.dark6
                height: 1
                width: searchPanel.width
            }
        }
    }
}
