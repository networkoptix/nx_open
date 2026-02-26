// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls as Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Core
import Nx.Core.Controls
import Nx.Items
import Nx.Mobile
import Nx.Mobile.Controls
import Nx.Screens
import Nx.Ui

import nx.vms.client.core

import "private"
import "private/items"

AdaptiveScreen
{
    id: screen

    objectName: "eventSearchScreen"

    property var customResourceId: null
    property QnCameraListModel camerasModel: null
    property alias analyticsSearchMode: screenController.analyticsSearchMode

    fullscreenToolBar: detailsLoader.item?.showControls ?? false
    title: contentItem.title
    longContent: contentItem !== eventSearchMenu

    customLeftControl: ToolBarButton
    {
        id: backButton

        anchors.centerIn: parent
        visible: state !== ""
        icon.source: "image://skin/24x24/Outline/arrow_back.svg?primary=light4"

        states:
        [
            State
            {
                name: "returnToSearchContent"
                when: screen.contentItem === filtersItem || screen.contentItem === detailsLoader.item

                PropertyChanges
                {
                    backButton.onClicked:
                    {
                        screen.contentItem = searchContent

                        if (!LayoutController.isTabletLayout)
                            detailsLoader.setSource("")

                        screen.fullscreen = false
                    }
                }
            },
            State
            {
                name: "returnToOptionSelector"
                when: screen.contentItem === optionSelectorItem

                PropertyChanges
                {
                    backButton.onClicked: { optionSelectorItem.apply(); screen.contentItem = filtersItem }
                }
            },
            State
            {
                name: "returnToPreviousScreen"
                when: stackView.depth > 1 &&
                    (LayoutController.isTablet
                        || (!LayoutController.isTabletLayout && screen.contentItem === searchContent))

                PropertyChanges
                {
                    backButton.onClicked: Workflow.popCurrentScreen()
                }
            },
            State
            {
                name: "returnToEventSearchMenu"
                when: screen.contentItem === searchContent

                PropertyChanges
                {
                    backButton.onClicked:
                    {
                        detailsLoader.setSource("")
                        screen.contentItem = eventSearchMenu
                    }
                }
            }
        ]
    }

    customRightControl: ToolBarButton
    {
        icon.source: "image://skin/24x24/Outline/filter_list.svg?primary=light4"
        visible: !LayoutController.isTabletLayout && screen.contentItem === searchContent
        onClicked:
        {
            screen.contentItem = filtersItem
        }
    }

    menuButton
    {
        visible: screen.contentItem === detailsLoader.item && detailsLoader.hasMenu
        icon.source: detailsLoader.icon
        onClicked: detailsLoader.item.share()
    }

    TextButton // TODO: Should be right control, but tool bar does not work well with wide buttons. Need to refactor tool bar.
    {
        id: clearAllButton

        parent: LayoutController.isTabletLayout ? leftPanel.availableHeaderArea : screen.toolBar
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Clear All")
        visible: LayoutController.isTabletLayout || screen.contentItem === filtersItem
        onClicked: screenController.clearFilters()
    }

    overlayItem: searchCanceller
    contentItem: customResourceId ? searchContent : eventSearchMenu

    leftPanel
    {
        title: qsTr("Filters")
        color: ColorTheme.colors.dark5
        iconSource: "image://skin/24x24/Outline/filter_list.svg?primary=dark1"
        interactive: true
        item: contentItem === searchContent ? leftPanelContainer : null
    }

    rightPanel
    {
        title: qsTr("Details")
        interactive: true
        color: ColorTheme.colors.dark4
        item: detailsLoader.item

        menuButton.visible: detailsLoader.hasMenu
        menuButton.icon.source: detailsLoader.icon
        menuButton.onClicked: detailsLoader.item.share()

        onItemChanged: rightPanel.visible = !!rightPanel.item
        onCloseButtonClicked: detailsLoader.setSource("")
    }

    Loader
    {
        id: detailsLoader

        readonly property var backend: item?.backend
        readonly property string icon:
        {
            if (!backend)
                return ""

            return backend.isShared && !screen.analyticsSearchMode
                ? "image://skin/20x20/Solid/shared.svg?primary=light4&secondary=green"
                : "image://skin/20x20/Solid/share.svg?primary=light4"
        }
        readonly property bool hasMenu: backend?.isAvailable || false

        Connections
        {
            target: detailsLoader.item ?? null

            function onFullscreenButtonClicked()
            {
                screen.fullscreen = !screen.fullscreen

                if (LayoutController.isTabletLayout)
                {
                    if (screen.contentItem === detailsLoader.item) //< Exit fullscreen.
                    {
                        screen.contentItem = searchContent
                        screen.rightPanel.visible = true
                    }
                    else //< Enter fullscreen.
                    {
                        screen.contentItem = detailsLoader.item
                    }
                }
                else
                {
                    if (CoreUtils.isMobilePlatform())
                    {
                        windowContext.ui.windowHelpers.setScreenOrientation(
                            LayoutController.isPortrait ? Qt.LandscapeOrientation : Qt.PortraitOrientation)
                    }
                    else
                    {
                        [mainWindow.width, mainWindow.height] = [mainWindow.height, mainWindow.width]
                    }
                }
            }

            function onSearchRequested(text)
            {
                searchField.text = text

                if (LayoutController.isPortrait)
                    screen.contentItem = searchContent
            }
        }
    }

    EventSearchMenu
    {
        id: eventSearchMenu

        QnCameraListModel
        {
            id: cameraListModel
        }

        onMenuItemClicked: (isAnalyticsSearchMode) =>
        {
            screen.customResourceId = undefined
            screen.camerasModel = cameraListModel
            screen.analyticsSearchMode = isAnalyticsSearchMode
            screen.contentItem = searchContent

            screenController.requestUpdate()
        }
    }

    SearchEdit
    {
        id: searchField

        implicitHeight: 36
        hasClearButton: false

        onDisplayTextChanged:
        {
            // TODO: call only when accepted to prevent unnecessary updates.
            screenController.searchSetup.textFilter.text = displayText
            screenController.requestUpdate()
        }
    }

    MouseArea
    {
        id: searchCanceller

        onPressed: (mouse) =>
        {
            mouse.accepted = false
            searchField.resetFocus()
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
        id: screenController

        property bool hasChanges: false

        Connections
        {
            target: screenController.searchSetup
            function onParametersChanged() { screenController.hasChanges = true }
        }

        Connections
        {
            target: screenController.analyticsSearchSetup
            function onParametersChanged() { screenController.hasChanges = true }
        }

        Connections
        {
            target: screenController.bookmarkSearchSetup
            function onParametersChanged() { screenController.hasChanges = true }
        }

        function updateIfRequired()
        {
            if (hasChanges)
            {
                requestUpdate()
                hasChanges = false
            }
        }

        function clearFilters()
        {
            // TODO: Should be more simple way generic. Now it does not work as expected. Selectors not updating.
            searchSetup.timeSelection = EventSearch.TimeSelection.anytime
            searchSetup.cameraSelection = EventSearch.CameraSelection.all
            if (bookmarkSearchSetup)
                bookmarkSearchSetup.searchSharedOnly = false

            updateIfRequired()
        }

        view: view
        refreshPreloader: Preloader
        {
            id: refreshPreloader

            parent: searchContent
            anchors.centerIn: parent
        }
    }

    FiltersItem
    {
        id: filtersItem

        controller: screenController
        onSelectorClicked: (selector) =>
        {
            optionSelectorItem.selector = selector
            if (LayoutController.isTabletLayout)
            {
                leftPanelPopupProxy.target = optionSelectorItem
                leftPanelPopup.open()
            }
            else
            {
                screen.contentItem = optionSelectorItem
            }
        }
    }

    OptionSelectorItem
    {
        id: optionSelectorItem

        onApplyRequested:
        {
            apply()
            screenController.updateIfRequired()

            if (LayoutController.isTabletLayout)
                leftPanelPopup.close()
            else
                screen.contentItem = filtersItem
        }
    }

    Item
    {
        id: leftPanelContainer

        LayoutItemProxy
        {
            id: landscapeSearchProxy

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 24
            target: LayoutController.isTabletLayout ? searchField : null
        }

        LayoutItemProxy
        {
            anchors.top: landscapeSearchProxy.bottom
            anchors.topMargin: 4
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom

            clip: true
            target: filtersItem
        }
    }

    Controls.Popup
    {
        id: leftPanelPopup

        x: screen.leftPanel.width
        height: screen.leftPanel.height
        width: 330
        visible: false
        modal: true
        padding: 0
        contentItem: Panel
        {
            color: ColorTheme.colors.dark5
            title: leftPanelPopupProxy.target?.title ?? ""
            onCloseButtonClicked: leftPanelPopup.close()

            LayoutItemProxy
            {
                id: leftPanelPopupProxy

                anchors.fill: parent
            }
        }

        onClosed:
        {
            optionSelectorItem.apply()
            screenController.updateIfRequired()
        }
    }

    Item
    {
        id: searchContent

        property string title: screenController.analyticsSearchMode ? qsTr("Objects") : qsTr("Bookmarks")

        LayoutItemProxy
        {
            id: portraitSearchProxy

            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            target: searchField
            visible: !LayoutController.isTabletLayout
        }

        ListView
        {
            id: view

            readonly property bool portraitMode: height > width

            y: portraitSearchProxy.visible ? searchField.height + 16 : 16
            height: parent.height - y
            width: parent.width

            clip: true
            spacing: 12

            boundsBehavior: Flickable.DragAndOvershootBounds

            model: screenController.searchModel

            delegate: EventSearchItem
            {
                isAnalyticsItem: screenController.analyticsSearchMode
                camerasModel: screen.camerasModel
                eventsModel: view.model
                currentEventIndex: index
                resource: model.resource
                previewId: model.previewId
                trackId: model.trackId?.toString() ?? ""
                previewState: model.previewState
                title: model.display
                extraText: model.description
                timestampMs: model.timestampMs
                shared: !!model.isSharedBookmark
                selected: LayoutController.isTabletLayout
                    && detailsLoader.item?.currentEventIndex === currentEventIndex

                onVisibleChanged: model.visible = visible

                onClicked:
                {
                    detailsLoader.setSource(
                        Qt.resolvedUrl("private/DetailsScreen.qml"),
                        {
                            "isAnalyticsDetails": isAnalyticsItem,
                            "camerasModel": camerasModel,
                            "eventSearchModel": eventsModel,
                            "currentEventIndex": currentEventIndex
                        })

                    if (!LayoutController.isTabletLayout)
                        screen.contentItem = detailsLoader.item
                }

                Component.onCompleted: model.visible = visible
                Component.onDestruction: model.visible = false
            }

            header: Preloader { topPreloader: true }
            footer: Preloader {}
        }

        Column
        {
            id: noDataPlaceholder

            spacing: 12
            width: Math.min(300, parent.width - 2 * 24)
            anchors.centerIn: parent
            visible: refreshPreloader && !refreshPreloader.visible && !view.count

            ColoredImage
            {
                primaryColor: textItem.color
                sourcePath: screenController.analyticsSearchMode
                    ? "image://skin/64x64/Outline/noobjects.svg"
                    : "image://skin/64x64/Outline/nobookmarks.svg"
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

                    text: screenController.analyticsSearchMode
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
                    text: screenController.analyticsSearchMode
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
            anchors.horizontalCenter: parent.horizontalCenter
            progress: screenController.refreshWatcher.refreshProgress
            y: view.y + screenController.refreshWatcher.overshoot * progress - height - view.spacing * 2
            z: -1
        }
    }

    Connections
    {
        id: layoutControllerConnections

        target: LayoutController

        function onIsPortraitChanged()
        {
            if (screen.contentItem === eventSearchMenu)
                return

            if (LayoutController.isMobile)
            {
                if (screen.contentItem !== detailsLoader.item)
                    return

                screen.fullscreen = !LayoutController.isPortrait
                return
            }

            if (LayoutController.isTabletLayout && screen.fullscreen)
                return

            if (LayoutController.isPortrait && screen.fullscreen)
            {
                screen.fullscreen = false
                return
            }

            leftPanelPopup.close()

            if (screen.contentItem === optionSelectorItem)
            {
                optionSelectorItem.apply()
                screenController.updateIfRequired()
            }

            screen.contentItem = searchContent
        }
    }

    Component.onCompleted:
    {
        if (customResourceId)
        {
            screenController.searchSetup.selectedCamerasIds = [customResourceId]
            screenController.requestUpdate()
        }
    }
}
