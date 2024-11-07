// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Core.Ui
import Nx.Common
import Nx.Items
import Nx.Mobile
import Nx.Ui

import nx.vms.client.core

import "private"
import "private/items"

Page
{
    id: screen

    objectName: "eventSearchScreen"

    property var customResourceId: null
    property QnCameraListModel camerasModel: null
    property alias analyticsSearchMode: controller.analyticsSearchMode

    title: screen.analyticsSearchMode
        ? qsTr("Objects")
        : qsTr("Bookmarks")

    onLeftButtonClicked: Workflow.popCurrentScreen()
    onHeaderClicked: searchButton.clicked()

    titleControls:
    [
        IconButton
        {
            id: searchButton

            padding: 0
            icon.source: lp("/images/search.png")
            onClicked:
            {
                sideNavigation.close()
                searchToolBar.open()
            }
            alwaysCompleteHighlightAnimation: false
        }
    ]

    SearchToolBar
    {
        id: searchToolBar
        parent: toolBar
        cancelIcon.source: lp("/images/close.svg")
        cancelIcon.width: 24
        cancelIcon.height: cancelIcon.width
        hasClearButton: false

        onAccepted:
        {
            controller.searchSetup.textFilter.text = text
            controller.requestUpdate()
        }

        onClosed:
        {
            controller.searchSetup.textFilter.text = ""
            controller.requestUpdate()
        }
    }

    component Preloader: Item
    {
        property bool topPreloader: false

        width: (parent && parent.width) || 0
        height: visible ? 46 : 0
        visible: false
        z: 1

        SpinnerBusyIndicator
        {
            readonly property real offset: topPreloader
                ? -view.spacing / 2
                : view.spacing / 2
            x: (parent.width - width) / 2
            y: (parent.height - height ) / 2 + offset
            running: parent.visible
        }
    }

    ScreenController
    {
        id: controller

        view: view
        refreshPreloader: Preloader
        {
            id: refreshPreloader

            parent: view.parent
            anchors.centerIn: parent
        }
    }

    FiltersPanel
    {
        id: filtersPanel

        controller: controller
        width: parent.width
    }

    ListView
    {
        id: view

        readonly property bool portraitMode: height > width

        y: filtersPanel.height + spacing
        width: parent.width
        height: parent.height - y - spacing

        clip: true
        spacing: 12

        boundsBehavior: Flickable.DragAndOvershootBounds

        model: controller.searchModel

        delegate: EventSearchItem
        {
            isAnalyticsItem: screen.analyticsSearchMode
            camerasModel: screen.camerasModel
            eventsModel: view.model
            currentEventIndex: index
            resource: model.resource
            previewId: model.previewId
            previewState: model.previewState
            title: model.display
            extraText: model.description
            timestampMs: model.timestampMs

            onVisibleChanged: model.visible = visible
            Component.onCompleted: model.visible = visible
            Component.onDestruction: model.visible = false
        }

        header: Preloader { topPreloader: true }
        footer: Preloader {}
    }

    Column
    {
        id: noDataPlaceholder

        parent: view.parent
        spacing: 12
        width: Math.min(300, parent.width - 2 * 24)
        anchors.centerIn: parent
        visible: refreshPreloader && !refreshPreloader.visible && !view.count

        ColoredImage
        {
            primaryColor: textItem.color
            sourcePath: controller.analyticsSearchMode
                ? "image://skin/analytics_icons/placeholders/no_objects.svg"
                : "image://skin/analytics_icons/placeholders/no_bookmarks.svg"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Column
        {
            spacing: 4
            width: parent.width
            anchors.horizontalCenter: parent.horizontalCenter

            Text
            {
                id: textItem

                text: controller.analyticsSearchMode
                    ? qsTr("No objects")
                    : qsTr("No bookmarks")

                width: Math.min(implicitWidth, parent.width)
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 18
                color: ColorTheme.colors.light16
            }

            Text
            {
                text: controller.analyticsSearchMode
                    ? qsTr("Try changing the filters or configure object detection in the camera plugin settings")
                    : qsTr("Try changing the filters to display the results")

                width: Math.min(implicitWidth, parent.width)
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: 14
                color: ColorTheme.colors.light16
                wrapMode: Text.WordWrap
            }
        }
    }


    RefreshIndicator
    {
        parent: controller.view.parent
        anchors.horizontalCenter: parent.horizontalCenter
        progress: controller.refreshWatcher.refreshProgress
        y: view.y + controller.refreshWatcher.overshoot * progress - height - view.spacing * 2
        z: -1
    }

    Component.onCompleted:
    {
        if (customResourceId)
            controller.searchSetup.selectedCamerasIds = [customResourceId]
    }
}
