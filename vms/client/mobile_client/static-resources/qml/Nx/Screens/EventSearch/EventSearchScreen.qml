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
import Nx.Models
import Nx.Screens
import Nx.Ui
import Nx.Models

import nx.vms.client.core
import nx.vms.client.mobile
import nx.vms.client.mobile.timeline

import "private"
import "private/items"

AdaptiveScreen
{
    id: screen

    objectName: "eventSearchScreen"

    property var customResourceId: null
    property QnCameraListModel camerasModel: null
    property alias analyticsSearchMode: screenController.analyticsSearchMode
    property alias searchText: searchField.text

    title: contentItem.title
    longContent: contentItem !== eventSearchMenu

    // The right (Details) panel mirrors `detailsLoader.item`. Clear the loader whenever
    // AdaptiveScreen closes the panel (close button, auto-close, or cross-close), otherwise
    // `rightPanel.item` stays bound to the same component and a repeated tap on the same row
    // produces no `onItemChanged` — the panel would not reopen.
    onPanelClosed: (panel) =>
    {
        if (panel === rightPanel)
            detailsLoader.setSource("")
    }

    function closeDetailsPanel()
    {
        if (!detailsLoader.item)
            return

        if (screen.contentItem === detailsLoader.item)
            screen.contentItem = searchContent

        detailsLoader.setSource("")

        if (screen.isActive)
            LayoutController.exitFullscreen()
    }

    customLeftControl: ToolBarButton
    {
        id: backButton

        anchors.centerIn: parent
        visible: state !== ""
        icon.source: "image://skin/24x24/Outline/arrow_back.svg?primary=light4"

        onClicked:
        {
            if (screen.customBackHandler)
                screen.customBackHandler()
        }

        states:
        [
            State
            {
                name: "returnToSearchContent"
                when: screen.contentItem === filtersItem || screen.contentItem === detailsLoader.item

                PropertyChanges
                {
                    screen.customBackHandler: () =>
                    {
                        screen.contentItem = searchContent

                        if (!LayoutController.isTabletLayout)
                            detailsLoader.setSource("")

                        LayoutController.exitFullscreen()
                    }
                }
            },
            State
            {
                name: "returnToOptionSelector"
                when: screen.contentItem === optionSelectorItem

                PropertyChanges
                {
                    screen.customBackHandler: () =>
                    {
                        optionSelectorItem.apply()
                        screenController.updateIfRequired()
                        screen.contentItem = filtersItem
                    }
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
                    screen.customBackHandler: () => Workflow.popCurrentScreen()
                }
            },
            State
            {
                name: "returnToEventSearchMenu"
                when: screen.contentItem === searchContent

                PropertyChanges
                {
                    screen.customBackHandler: () =>
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
        indicator.visible: filtersItem.hasActiveFilters
        onClicked:
        {
            screen.contentItem = filtersItem
        }
    }

    menuButton
    {
        visible: !!detailsLoader.menu && screen.contentItem === detailsLoader.item
        onClicked: detailsLoader.menu.popup(menuButton, 0, menuButton.height)
    }

    TextButton // TODO: Should be right control, but tool bar does not work well with wide buttons. Need to refactor tool bar.
    {
        id: resetAllButton

        parent: LayoutController.isTabletLayout ? leftPanel.availableHeaderArea : screen.toolBar
        anchors.right: parent.right
        anchors.topMargin: LayoutController.isTabletLayout ? 20 : 0
        anchors.rightMargin: LayoutController.isTabletLayout ? 0 : 20
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Reset All")
        visible: (LayoutController.isTabletLayout || screen.contentItem === filtersItem)
            && filtersItem.hasActiveFilters
        onClicked: screenController.clearFilters()
    }

    TextButton // TODO: Should be right control, but tool bar does not work well with wide buttons. Need to refactor tool bar.
    {
        id: clearButton

        parent: screen.toolBar
        anchors.right: parent.right
        anchors.rightMargin: 20
        anchors.verticalCenter: parent.verticalCenter

        text: qsTr("Reset")
        visible: !LayoutController.isTabletLayout
            && screen.contentItem === optionSelectorItem
            && !!optionSelectorItem.selector
            && !optionSelectorItem.selector.isDefaultValue

        onClicked:
        {
            optionSelectorItem.clear()
            if (optionSelectorItem.closesOnApply)
            {
                optionSelectorItem.apply()
                screenController.updateIfRequired()
                screen.contentItem = filtersItem
            }
        }
    }

    overlayItem: searchCanceller
    contentItem: customResourceId ? searchContent : eventSearchMenu

    leftPanel
    {
        title: qsTr("Filters")
        color: ColorTheme.colors.dark5
        iconSource: "image://skin/24x24/Outline/filter_list.svg?primary=dark1"
        interactive: true
        visible: true
        item: contentItem === searchContent ? leftPanelContainer : null
    }

    leftPanelButtonIndicator.visible: filtersItem.hasActiveFilters

    rightPanel
    {
        title: qsTr("Details")
        interactive: true
        color: ColorTheme.colors.dark4
        item: detailsLoader.item

        menuButton.visible: !!detailsLoader.menu
        menuButton.onClicked: detailsLoader.menu.popup(rightPanel.menuButton, 0, menuButton.height)

        onItemChanged:
        {
            if (rightPanel.item)
                screen.showPanel(rightPanel)
            // Hiding when item becomes null is handled by AdaptiveScreen's
            // `Binding { when: !rightPanel.item; visible: false }`.
        }
    }

    Loader
    {
        id: detailsLoader

        readonly property var menu: item?.menu

        Connections
        {
            target: detailsLoader.item ?? null

            function onBackClicked()
            {
                if (screen.customBackHandler)
                    screen.customBackHandler()
            }

            function onSearchRequested(text)
            {
                Workflow.openEventSearchScreen(
                    /*push*/ true,
                    /*selectedResourceId*/ null,
                    screen.camerasModel,
                    screen.analyticsSearchMode,
                    /*searchText*/ text)
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

            searchField.clear()
            filtersItem.clearAll()

            screen.contentItem = searchContent

            screenController.requestUpdate()
        }
    }

    SearchEdit
    {
        id: searchField

        implicitHeight: 36

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
            filtersItem.clearAll()
            updateIfRequired()
        }

        view: view

        refreshPreloader: Skeleton
        {
            id: refreshPreloader

            parent: searchContent

            anchors.fill: parent
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            anchors.topMargin: LayoutController.isTabletLayout ? 20 : 68

            ColumnLayout
            {
                anchors.fill: parent
                spacing: LayoutController.isTablet ? 12 : 8

                Repeater
                {
                    model: 8

                    Rectangle
                    {
                        Layout.fillWidth: true
                        Layout.preferredHeight: LayoutController.isTablet ? 209 : 92 //< Keep in sync with EventSearchItem height.

                        radius: 8
                        color: "white"
                        antialiasing: false
                    }
                }
            }
        }
    }

    ModelDataAccessor
    {
        id: searchModelAccessor
        model: screenController.searchModel
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
                leftPanelPopupContent.item = optionSelectorItem
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

        // In tablet mode, even intermediate filter values are expected to be applied immediately.
        Connections
        {
            target: optionSelectorItem.selector
            enabled: leftPanelPopup.opened

            function onIntermediateValueChanged()
            {
                optionSelectorItem.apply()
                screenController.updateIfRequired()
            }
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
            anchors.leftMargin: 20
            anchors.rightMargin: 20
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

        parent: screen.leftPanel
        x: screen.leftPanel.width + screen.spacing
        height: screen.leftPanel.height
        width: StyleHints.sheetWidth
        closePolicy: Controls.Popup.CloseOnPressOutsideParent | Controls.Popup.CloseOnEscape
        visible: false
        modal: false
        padding: 0
        contentItem: Panel
        {
            id: leftPanelPopupContent

            color: ColorTheme.colors.dark5
            title: item?.title ?? ""
            onCloseButtonClicked: leftPanelPopup.close()

            TextButton
            {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.verticalCenter: parent.verticalCenter

                parent: leftPanelPopupContent.availableHeaderArea
                padding: 0
                text: qsTr("Reset")
                visible: optionSelectorItem.selector && !optionSelectorItem.selector.isDefaultValue
                onClicked:
                {
                    optionSelectorItem.clear()
                    if (optionSelectorItem.closesOnApply)
                        leftPanelPopup.close()
                }
            }
        }

        onClosed:
        {
            optionSelectorItem.apply()
            screenController.updateIfRequired()
        }

        Connections
        {
            target: screen.leftPanel
            enabled: leftPanelPopup.visible

            function onVisibleChanged()
            {
                if (!screen.leftPanel.visible)
                    leftPanelPopup.close()
            }
        }
    }

    CustomPopupDimmer
    {
        id: filterPopupDimmer

        anchors.fill: parent
        anchors.leftMargin: leftPanelPopup.x + leftPanelPopup.width

        parent: screen
        popup: leftPanelPopup
        color: ColorTheme.transparent(ColorTheme.colors.dark1, 0.5)
    }

    Item
    {
        id: searchContent

        property string title: screenController.analyticsSearchMode ? qsTr("Objects") : qsTr("Bookmarks")

        LayoutItemProxy
        {
            id: portraitSearchProxy

            anchors.top: parent.top
            anchors.topMargin: 16
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

            y: portraitSearchProxy.visible ? searchField.height + 32 : 20
            height: parent.height - y
            width: parent.width
            leftMargin: 20
            rightMargin: 20

            clip: true
            spacing: LayoutController.isTablet ? 12 : 8

            boundsBehavior: Flickable.DragAndOvershootBounds

            model: screenController.searchModel

            delegate: EventSearchItem
            {
                uuid: model.uuid
                isAnalyticsItem: screenController.analyticsSearchMode
                camerasModel: screen.camerasModel
                eventsModel: view.model
                currentEventIndex: index
                resource: model.resource
                previewId: model.previewId
                previewAspectRatio: model.previewAspectRatio
                trackId: model.trackId?.toString() ?? ""
                previewState: model.previewState
                title: model.display
                extraText: model.description
                timestampMs: model.timestampMs
                shared: !!model.isSharedBookmark
                selected: LayoutController.isTabletLayout && detailsLoader.item?.uuid === uuid

                onVisibleChanged: model.visible = visible

                onClicked: view.currentIndex = index

                Component.onCompleted: model.visible = visible
                Component.onDestruction: model.visible = false
            }

            onCurrentIndexChanged:
            {
                if (currentIndex === -1)
                    return

                const modelIndex = view.model.index(currentIndex, 0)
                const resource = NxGlobals.modelData(modelIndex, "resource")
                const objectData = ObjectDataAdapter.create(modelIndex)

                detailsLoader.setSource(
                    Qt.resolvedUrl("../private/DetailsScreen.qml"),
                    {
                        "uuid": NxGlobals.modelData(modelIndex, "uuid"),
                        "objectsType": screenController.analyticsSearchMode
                            ? ObjectsLoader.ObjectsType.analytics
                            : ObjectsLoader.ObjectsType.bookmarks,
                        "camerasModel": screen.camerasModel,
                        "objectData": objectData,
                        "hasNext": Qt.binding(() => view.currentIndex > 0),
                        "hasPrevious": Qt.binding(() => view.currentIndex < view.count - 1),
                        "resource": resource
                    })

                if (!LayoutController.isTabletLayout)
                    screen.contentItem = detailsLoader.item
            }

            Binding on currentIndex
            {
                when: !detailsLoader.item
                value: -1
            }

            Connections
            {
                target: detailsLoader.item

                function onNextClicked() { view.decrementCurrentIndex() }
                function onPreviousClicked() { view.incrementCurrentIndex() }
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
        target: screenController.searchModel
        enabled: !!detailsLoader.item

        function onRowsRemoved()
        {
            validateSelectedItem()
        }

        function validateSelectedItem()
        {
            const selectedUuid = detailsLoader.item?.uuid
            if (!selectedUuid)
                return

            for (let i = 0; i < searchModelAccessor.count; ++i)
            {
                if (searchModelAccessor.getData(i, "uuid") == selectedUuid)
                {
                    view.currentIndex = i
                    return
                }
            }

            screen.closeDetailsPanel()
        }

        function onFetchFinished(result, centralItemIndex, request)
        {
            if (result === EventSearch.FetchResult.failed
                || result === EventSearch.FetchResult.cancelled)
            {
                return
            }

            validateSelectedItem()
        }
    }

    Connections
    {
        id: layoutControllerConnections

        target: LayoutController

        function onFullscreenChanged()
        {
            if (!screen.isActive)
                return

            // On the mobile layout the details are the content item already; only the tablet
            // layout swaps the content between the search results and the details.
            if (!LayoutController.isTabletLayout)
                return

            if (LayoutController.fullscreen)
            {
                if (detailsLoader.item)
                    screen.contentItem = detailsLoader.item
            }
            else if (screen.contentItem === detailsLoader.item)
            {
                screen.contentItem = searchContent
                screen.showPanel(rightPanel)
            }
        }

        function onIsPortraitChanged()
        {
            if (!screen.isActive)
                return

            if (screen.contentItem === eventSearchMenu)
                return

            if (LayoutController.isMobile)
                return

            if (LayoutController.isPortrait)
            {
                if (screen.contentItem !== detailsLoader.item)
                    screen.closeDetailsPanel()

                return
            }

            if (LayoutController.fullscreen)
                return

            leftPanelPopup.close()

            if (screen.contentItem === optionSelectorItem)
            {
                optionSelectorItem.apply()
                screenController.updateIfRequired()
            }

            screen.closeDetailsPanel()
            screen.contentItem = searchContent
        }
    }

    Component.onCompleted:
    {
        if (screen.searchText)
            screen.contentItem = searchContent

        if (customResourceId)
        {
            screenController.searchSetup.selectedCamerasIds = [customResourceId]
            screenController.requestUpdate()
        }
    }
}
