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
        icon.source: "image://skin/24x24/Outline/arrow_back.svg?primary=%1"
            .arg(StyleHints.foregroundColorName)

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

                        if (!LayoutController.hasSidePanels)
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
                when: stackView.depth > 1
                    && (LayoutController.hasSidePanels || screen.contentItem === searchContent)

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
        icon.source: "image://skin/24x24/Outline/filter_list.svg?primary=%1"
            .arg(StyleHints.foregroundColorName)
        visible: !LayoutController.hasSidePanels && screen.contentItem === searchContent
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

        parent: LayoutController.hasSidePanels ? leftPanel.availableHeaderArea : screen.toolBar
        anchors.right: parent.right
        anchors.topMargin: LayoutController.hasSidePanels ? 20 : 0
        anchors.rightMargin: LayoutController.hasSidePanels ? 0 : 20
        anchors.verticalCenter: parent.verticalCenter
        text: qsTr("Reset All")
        visible: (LayoutController.hasSidePanels || screen.contentItem === filtersItem)
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
        visible: !LayoutController.hasSidePanels
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
        title: detailsLoader.item?.resource?.name ?? qsTr("Details")
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

            function onShowOnCameraRequested(resource, timestampMs)
            {
                Workflow.openVideoScreen(
                    resource,
                    timestampMs,
                    screen.camerasModel,
                    screen.analyticsSearchMode
                        ? ObjectsLoader.ObjectsType.analytics
                        : ObjectsLoader.ObjectsType.bookmarks,
                    /*isAuxiliary*/ true)
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
        id: preloader

        property bool topPreloader: false
        property bool loading: false //< Shown while loading and until it collapses afterwards.

        // Room between the rows and the view edge, negative when the preloader is out of view.
        function exposedHeight()
        {
            return preloader.topPreloader
                ? preloader.y + preloader.height - view.contentY
                : view.contentY + view.height - preloader.y
        }

        function collapse()
        {
            if (preloader.loading || preloader.height === 0 || heightAnimation.running)
                return

            const exposed = preloader.exposedHeight()
            if (exposed < 0)
            {
                preloader.height = 0
                return
            }

            // The view returns from beyond the preloader: collapse when the rows reach it.
            if (exposed > preloader.height + 0.5)
                return

            heightAnimation.to = 0
            heightAnimation.restart()
        }

        width: (parent && parent.width) || 0
        height: 0
        visible: preloader.loading || preloader.height > 0
        z: 1

        onLoadingChanged:
        {
            heightAnimation.stop()

            if (!preloader.loading)
            {
                preloader.collapse()
                return
            }

            // Requested with the room for it in the overshoot already: nothing moves.
            preloader.height = view.preloaderHeight
        }

        onHeightChanged:
        {
            if (!heightAnimation.running || view.dragging)
                return

            // Collapsing, the view is kept in its bounds here: Qt does it only when the view is
            // not moving, and a return to the bounds restarted by every step of the collapse stalls.
            const beyond = preloader.exposedHeight() - preloader.height
            if (beyond > 0)
                view.contentY += preloader.topPreloader ? beyond : -beyond
        }

        Connections
        {
            target: view

            function onContentYChanged() { preloader.collapse() }
        }

        NumberAnimation on height
        {
            id: heightAnimation

            running: false
            duration: 200
            easing.type: Easing.OutCubic
        }

        SpinnerBusyIndicator
        {
            // At a fixed distance from the rows, like the refresh indicator: it does not move
            // relative to them while the preloader grows or collapses.
            readonly property real offset: preloader.topPreloader
                ? preloader.height - view.preloaderHeight - view.spacing / 2
                : view.spacing / 2
            x: (preloader.width - width) / 2
            y: (view.preloaderHeight - height) / 2 + offset
            running: preloader.visible
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
            anchors.topMargin: LayoutController.hasSidePanels ? 20 : 68

            ColumnLayout
            {
                anchors.fill: parent
                spacing: LayoutController.isExpanded ? 12 : 8

                Repeater
                {
                    model: 8

                    Rectangle
                    {
                        Layout.fillWidth: true
                        // Keep in sync with EventSearchItem height: padding + 16:9 preview,
                        // which in the expanded mode is limited by the description block
                        // minimum width.
                        Layout.preferredHeight:
                        {
                            if (!LayoutController.isExpanded)
                                return 92

                            const padding = 20
                            const spacing = 20
                            const descriptionMinimumWidth = 180
                            const previewWidth = Math.max(0, Math.min(300,
                                width - padding * 2 - spacing - descriptionMinimumWidth))

                            return previewWidth * 9 / 16 + padding * 2
                        }

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
            if (LayoutController.hasSidePanels)
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

            if (LayoutController.hasSidePanels)
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
            id: panelSearchProxy

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 20
            anchors.rightMargin: 20
            target: LayoutController.hasSidePanels ? searchField : null
        }

        LayoutItemProxy
        {
            anchors.top: panelSearchProxy.bottom
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
            id: contentSearchProxy

            anchors.top: parent.top
            anchors.topMargin: 16
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            target: searchField
            visible: !LayoutController.hasSidePanels
        }

        ListView
        {
            id: view

            readonly property real preloaderHeight: screenController.refreshWatcher.topOvershoot

            y: contentSearchProxy.visible ? searchField.height + 32 : 20
            height: parent.height - y
            width: parent.width
            leftMargin: 20
            rightMargin: 20

            clip: true
            spacing: LayoutController.isExpanded ? 12 : 8

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
                selected: LayoutController.hasSidePanels && detailsLoader.item?.uuid === uuid

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
                    Qt.resolvedUrl("../private/DetailsScreen/DetailsItem.qml"),
                    {
                        "uuid": NxGlobals.modelData(modelIndex, "uuid"),
                        "objectsType": screenController.analyticsSearchMode
                            ? ObjectsLoader.ObjectsType.analytics
                            : ObjectsLoader.ObjectsType.bookmarks,
                        "objectData": objectData,
                        "hasNext": Qt.binding(() => view.currentIndex > 0),
                        "hasPrevious": Qt.binding(() => view.currentIndex < view.count - 1),
                        "resource": resource,
                        "showPreviewImage": Qt.binding(() => !LayoutController.hasSidePanels),
                        "gestureExclusionEnabled": Qt.binding(() => LayoutController.isPortrait
                            && screen.isActive
                            && screen.contentItem === detailsLoader.item)
                    })

                if (!LayoutController.hasSidePanels)
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
            // In the list viewport, not over the search field. Centered where the top preloader
            // spinner appears: the spinner replaces it in place when the fetch starts.
            parent: view
            anchors.horizontalCenter: parent.horizontalCenter
            y: screenController.refreshWatcher.overshoot
                - (view.preloaderHeight + view.spacing + height) / 2
            z: -1
            visible: !view.headerItem?.loading
            progress: screenController.refreshWatcher.refreshProgress
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
            if (!LayoutController.hasSidePanels)
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

        // Keyed on the shell kind rather than on `mode`: moving between two layouts that both
        // have side panels must leave the panels alone.
        function onHasSidePanelsChanged()
        {
            if (!screen.isActive)
                return

            if (screen.contentItem === eventSearchMenu)
                return

            if (!LayoutController.hasSidePanels)
            {
                // The details were living in the right panel, which is gone now. Releasing them
                // resets `view.currentIndex` through the binding below, so the row that was
                // selected before the resize can be tapped open again - re-selecting the current
                // index emits no change and would leave the row dead.
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
