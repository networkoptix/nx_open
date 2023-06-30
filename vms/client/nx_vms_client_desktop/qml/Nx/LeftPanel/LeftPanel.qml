// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import Nx.Core 1.0
import Nx.Controls 1.0
import Nx.Items 1.0

import nx.vms.client.core 1.0
import nx.vms.client.desktop 1.0

import "private"
import "private/analytics"

import "private/metrics.js" as Metrics

Item
{
    id: leftPanel

    readonly property alias currentTab: d.currentTab

    property real maximumWidth: 0
    readonly property real minimumWidth: d.extraWidth + d.minimumContentWidth
    width: d.constrainedWidth
    height: 600 //< Some sensible default.

    readonly property alias resourceBrowser: resourceBrowser
    readonly property alias motionSearchPanel: motionSearchPanel
    readonly property alias bookmarkSearchPanel: bookmarkSearchPanel
    readonly property alias eventSearchPanel: eventSearchPanel
    readonly property alias objectSearchPanel: objectSearchPanel
    readonly property alias settingsPanel: settingsPanel

    // Enable/disable all resizers (used during animated open/close operations).
    property bool resizeEnabled: true

    // Spacing between the button bar and pages, zero by default.
    property alias spacing: leftPanelRow.spacing

    // The rightmost padding produced by a resizer, if it exists in a current page.
    readonly property real resizerPadding:
        d.hasRightmostResizer(d.currentTab) ? Metrics.kResizerWidth : 0

    layer.enabled: opacity < 1.0

    function setCurrentTab(tab)
    {
        d.setCurrentTab(tab)
    }

    function setShortcutHintText(tab, value)
    {
        const button = buttonGroup.buttonByTab(tab)
        if (button)
            button.shortcutHintText = value
    }

    Row
    {
        id: leftPanelRow

        spacing: 0
        anchors.fill: leftPanel

        Rectangle
        {
            id: buttonBar

            property IndexButton hoveredButton: null

            z: 1 //< regular/narrow resizer should go above the next item.
            property bool narrow: true

            height: leftPanel.height
            width: buttonsLayout.width

            color: ColorTheme.colors.dark4

            ButtonGroup
            {
                id: buttonGroup

                readonly property var indexByTab: Array.prototype.reduce.call(buttonGroup.buttons,
                    function(result, button, index)
                    {
                        result[button.tab] = index
                        return result
                    },
                    {})

                function buttonByTab(tab)
                {
                    return buttonGroup.buttons[buttonGroup.indexByTab[tab]] ?? null
                }

                buttons: buttonsLayout.children //< Spacer will be properly filtered out.
                exclusive: true

                onClicked:
                {
                    d.setCurrentTab(d.currentTab !== button.tab
                        ? button.tab
                        : RightPanel.Tab.invalid)
                }
            }

            ColumnLayout
            {
                id: buttonsLayout

                y: 8
                height: buttonBar.height - y - 8
                spacing: 0

                property alias d: d

                component TabButton: IndexButton
                {
                    id: tabButton

                    property bool allowed: true
                    property RightPanelModel model: null

                    visible: allowed
                    narrow: buttonBar.narrow

                    onHoveredChanged:
                    {
                        if (tabButton.hovered)
                            buttonBar.hoveredButton = tabButton
                        else if (buttonBar.hoveredButton === tabButton)
                            buttonBar.hoveredButton = null
                    }

                    onAllowedChanged:
                    {
                        if (!d) //< Might be called before the component is complete.
                            return
                        if (d.currentTab === tabButton.tab && model.isOnline)
                            d.setCurrentTab(d.previousTab)
                    }
                }

                TabButton
                {
                    text: resourceBrowser.tree.model.localFilesMode
                        ? qsTr("Local Files")
                        : qsTr("Cameras") + "<br>& " + qsTr("Resources")

                    icon.source: resourceBrowser.tree.model.localFilesMode
                        ? "image://svg/skin/left_panel/localfiles.svg"
                        : "image://svg/skin/left_panel/camera.svg"

                    checked: true //< By default.
                    tab: { return RightPanel.Tab.resources }
                }

                Item
                {
                    id: separator

                    height: 16
                    Layout.fillWidth: true

                    Rectangle
                    {
                        anchors.centerIn: parent
                        width: buttonBar.narrow ? 32 : 56
                        height: 1
                        color: ColorTheme.colors.dark9
                    }
                }

                TabButton
                {
                    text: qsTr("Motion")
                    icon.source: "image://svg/skin/left_panel/motion.svg"
                    model: motionSearchPanel.model
                    allowed: model.isAllowed
                    tab: { return RightPanel.Tab.motion }
                }

                TabButton
                {
                    text: qsTr("Bookmarks")
                    icon.source: "image://svg/skin/left_panel/bookmarks.svg"
                    model: bookmarkSearchPanel.model
                    allowed: model.isAllowed
                    tab: { return RightPanel.Tab.bookmarks }
                }

                TabButton
                {
                    text: qsTr("Events")
                    icon.source: "image://svg/skin/left_panel/events.svg"
                    model: eventSearchPanel.model
                    allowed: model.isAllowed
                    tab: { return RightPanel.Tab.events }
                }

                TabButton
                {
                    text: qsTr("Objects")
                    icon.source: "image://svg/skin/left_panel/objects.svg"
                    model: objectSearchPanel.model
                    allowed: model.isAllowed
                    tab: { return RightPanel.Tab.analytics }
                }

                Item
                {
                    id: spacer
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                }

                TabButton
                {
                    allowed: false //< Not implemented yet, hide for now.
                    text: qsTr("Settings")
                    icon.source: "image://svg/skin/left_panel/settings.svg"
                    tab: { return RightPanel.Tab.settings }
                }

                // Tooltip for narrow buttons.
                BubbleToolTip
                {
                    id: toolTip

                    readonly property alias button: buttonBar.hoveredButton

                    onButtonChanged:
                    {
                        if (!button || !buttonBar.narrow || !button.text)
                        {
                            hide()
                            return
                        }

                        target = button
                        text = getText(button)

                        if (state !== BubbleToolTip.Suppressed)
                            show()
                    }

                    function getText(button)
                    {
                        if (!button)
                            return ""

                        // Button text may have a manual break (because of limited width),
                        // but for the tooltip we want a single line of text.
                        const buttonText = button.text.replace("<br>", " ")
                        const hotkeyText = button.shortcutHintText

                        if (!hotkeyText)
                            return buttonText

                        const hotkeyColor = ColorTheme.colors.light16
                        return buttonText
                            + ` <font color="${hotkeyColor}">(${hotkeyText})</font>`
                    }
                }
            }

            MouseArea
            {
                id: buttonBarResizer

                x: buttonBar.width - width / 2
                y: 0
                width: 8
                height: parent.height
                enabled: leftPanel.resizeEnabled
                acceptedButtons: Qt.LeftButton
                hoverEnabled: true //< Avoid highlighting underlying items.

                cursorShape: Qt.SplitHCursor
                CursorOverride.shape: cursorShape
                CursorOverride.active: pressed

                onMouseXChanged:
                {
                    if (!pressed)
                        return

                    const threshold = 16
                    const distance = mouseX - width / 2
                    if (Math.abs(distance) > threshold)
                        buttonBar.narrow = distance < 0
                }
            }
        }

        Stack
        {
            id: pages

            width: d.currentItem ? d.currentItem.width : 0
            height: leftPanel.height
            visible: !!d.currentItem

            currentIndex: buttonGroup.indexByTab[d.currentTab] ?? -1

            Item
            {
                id: resourcePanel
                objectName: "ResourcePanel"

                width: resourceBrowser.width + resourseBrowserResizer.width
                height: pages.height

                ResourceBrowser
                {
                    id: resourceBrowser
                    objectName: "ResourceBrowser"

                    property real desiredWidth: Metrics.kDefaultResourceBrowserWidth

                    readonly property real maximumWidth: Math.min(
                        Metrics.kMaximumResourceBrowserWidth,
                        d.maximumContentWidth - resourseBrowserResizer.width)

                    width: MathUtils.bound(Metrics.kMinimumResourceBrowserWidth,
                        resourceBrowser.desiredWidth, resourceBrowser.maximumWidth)

                    height: resourcePanel.height
                }

                Resizer
                {
                    id: resourseBrowserResizer

                    z: 1
                    edge: Qt.RightEdge
                    target: resourceBrowser
                    enabled: leftPanel.resizeEnabled
                    cursorShape: Qt.SizeHorCursor
                    handleWidth: Metrics.kResizerWidth
                    drag.minimumX: Metrics.kMinimumResourceBrowserWidth
                    drag.maximumX: resourceBrowser.maximumWidth

                    onDragPositionChanged:
                        resourceBrowser.desiredWidth = pos
                }
            }

            SearchPanel
            {
                id: motionSearchPanel
                objectName: "MotionPanel"

                type: { return EventSearch.SearchType.motion }

                width: Metrics.kFixedSearchPanelWidth
                height: pages.height
                brief: true
                limitToCurrentCamera: true

                placeholder
                {
                    icon: "image://svg/skin/left_panel/placeholders/motion.svg"

                    title: placeholderAction.visible
                        ? qsTr("Select a camera to see its motion events")
                        : qsTr("No motion detected")

                    description: placeholderAction.visible
                        ? ""
                        : qsTr("Try changing the filters or enable motion recording")

                    action: Action
                    {
                        id: placeholderAction

                        text: qsTr("Select Camera...")
                        icon.source: "qrc:///skin/tree/cameras.svg"

                        visible: d.isOnline
                            && resourceBrowser.scene.itemCount == 0
                            && !resourceBrowser.scene.isLocked
                            && !resourceBrowser.scene.isShowreelReviewLayout

                        onTriggered:
                            motionSearchPanel.model.addCameraToLayout()
                    }
                }

                MotionAreaSelector
                {
                    motionModel: motionSearchPanel.model.sourceModel
                    parent: motionSearchPanel.filtersColumn
                    width: parent.width
                }

                onActiveChanged:
                    d.setTabState(RightPanel.Tab.motion, /*isCurrent*/ active)
            }

            SearchPanel
            {
                id: bookmarkSearchPanel
                objectName: "BookmarksPanel"

                type: { return EventSearch.SearchType.bookmarks }

                width: Metrics.kFixedSearchPanelWidth
                height: pages.height

                placeholder
                {
                    icon: "image://svg/skin/left_panel/placeholders/bookmarks.svg"
                    title: qsTr("No bookmarks")
                    description: qsTr("Select a time span on the timeline and right-click the "
                        + "highlighted section to create a bookmark")
                }

                onActiveChanged:
                    d.setTabState(RightPanel.Tab.bookmarks, /*isCurrent*/ active)
            }

            EventsPanel
            {
                id: eventSearchPanel
                objectName: "EventsPanel"

                maximumWidth: d.maximumContentWidth
                resizeEnabled: leftPanel.resizeEnabled
                height: pages.height

                onDesiredFiltersWidthChanged:
                    d.setFilterColumnWidth(eventSearchPanel.desiredFiltersWidth)

                onActiveChanged:
                    d.setTabState(RightPanel.Tab.events, /*isCurrent*/ active)
            }

            AnalyticsPanel
            {
                id: objectSearchPanel
                objectName: "AnalyticsPanel"

                maximumWidth: d.maximumContentWidth
                resizeEnabled: leftPanel.resizeEnabled
                height: pages.height
                showInformation: false //< Default value.

                onDesiredFiltersWidthChanged:
                    d.setFilterColumnWidth(objectSearchPanel.desiredFiltersWidth)

                onActiveChanged:
                    d.setTabState(RightPanel.Tab.analytics, /*isCurrent*/ active)
            }

            Rectangle
            {
                id: settingsPanel
                objectName: "SettingsPanel"

                color: "grey"
                width: 320
                height: pages.height
            }
        }

        ClientStateDelegate
        {
            id: stateDelegate

            name: "SearchPanel"

            // This tab takes effect only online, i.e. when the client is connected to any system.
            property int currentTab: { return RightPanel.Tab.resources }

            property alias buttonBarNarrow: buttonBar.narrow

            property alias resourceBrowserWidth: resourceBrowser.desiredWidth

            property alias motionThumbnails: motionSearchPanel.showThumbnails
            property alias bookmarkThumbnails: bookmarkSearchPanel.showThumbnails
            property alias eventThumbnails: eventSearchPanel.showThumbnails

            // Synchronized for Events and Objects.
            property alias filterColumnWidth: eventSearchPanel.desiredFiltersWidth

            property alias objectFiltersWidth: objectSearchPanel.desiredFiltersWidth
            property alias objectResultColumns: objectSearchPanel.desiredResultColumns
            property alias objectThumbnails: objectSearchPanel.showThumbnails
            property alias objectsInformation: objectSearchPanel.showInformation

            onStateAboutToBeSaved:
            {
                // System-independent state can be saved before disconnecting.
                if (d.isOnline)
                    stateDelegate.currentTab = d.currentTab
            }

            onStateLoaded:
            {
                if (d.isOnline)
                    d.setCurrentTab(stateDelegate.currentTab)

                const pageIndex = buttonGroup.indexByTab[stateDelegate.currentTab]
                const page = pages.children[pageIndex]

                if (LocalSettings.iniConfigValue("newPanelsLayout"))
                {
                    reportStatistics("left_panel_active_tab", page.objectName)
                    reportStatistics("left_panel_motion_thumbnails", motionThumbnails)
                    reportStatistics("left_panel_bookmark_thumbnails", bookmarkThumbnails)
                    reportStatistics("left_panel_event_thumbnails", eventThumbnails)
                    reportStatistics("left_panel_analytics_thumbnails", objectThumbnails)
                    reportStatistics("left_panel_analytics_information", objectsInformation)
                    const width = tabWidth(currentTab)
                    if (width != undefined)
                        reportStatistics("left_panel_width", width)
                }
            }

            function tabWidth(tab)
            {
                switch (tab)
                {
                    case RightPanel.Tab.resources: return resourceBrowserWidth
                    case RightPanel.Tab.events: return filterColumnWidth
                    case RightPanel.Tab.analytics: return objectFiltersWidth
                }
                return undefined
            }
        }

        NxObject
        {
            id: d

            property int currentTab: { return RightPanel.Tab.resources }
            property int previousTab: { return RightPanel.Tab.invalid }

            readonly property Item currentItem: pages.children[pages.currentIndex] ?? null

            readonly property real extraWidth:
                pages.x + (currentItem ? leftPanelRow.spacing : 0)

            readonly property real totalWidth: d.extraWidth + d.contentWidth

            readonly property real constrainedWidth: leftPanel.maximumWidth
                ? Math.min(d.totalWidth, leftPanel.maximumWidth)
                : d.totalWidth

            readonly property real minimumContentWidth: d.currentItem
                ? (d.currentItem.minimumWidth + leftPanelRow.spacing)
                : 0

            readonly property real contentWidth: d.currentItem
                ? (d.currentItem.width + leftPanelRow.spacing)
                : 0

            readonly property real maximumContentWidth: leftPanel.maximumWidth
                ? (leftPanel.maximumWidth - d.extraWidth)
                : Number.MAX_VALUE

            readonly property bool isOnline: !resourceBrowser.tree.model.localFilesMode

            onIsOnlineChanged:
            {
                if (isOnline)
                {
                    d.setCurrentTab(stateDelegate.currentTab)
                }
                else
                {
                    stateDelegate.currentTab = d.currentTab
                    d.setCurrentTab(RightPanel.Tab.resources)
                }
            }

            function setFilterColumnWidth(value)
            {
                eventSearchPanel.desiredFiltersWidth = value
                objectSearchPanel.desiredFiltersWidth = value
            }

            function setCurrentTab(tab)
            {
                const button = buttonGroup.buttonByTab(tab)

                // Whether it's an attempt to set a disallowed tab...
                if (button && !button.allowed)
                {
                    // If current tab is allowed, just do nothing.
                    if (!buttonGroup.checkedButton || buttonGroup.checkedButton.allowed)
                        return

                    // Otherwise first try to fall back to previous tab, then to no tab.
                    d.setCurrentTab(d.previousTab !== tab ? d.previousTab : RightPanel.Tab.invalid)
                    return
                }

                const effectiveTab = button ? button.tab : RightPanel.Tab.invalid
                if (effectiveTab === d.currentTab)
                    return

                d.previousTab = d.currentTab
                d.currentTab = effectiveTab

                for (let i = 0; i < buttonGroup.buttons.length; ++i)
                {
                    const tab = buttonGroup.buttons[i].tab
                    const panel = pages.children[buttonGroup.indexByTab[tab]]

                    if (panel.hasOwnProperty("active"))
                        panel.active = tab === effectiveTab
                }

                if (button)
                    button.checked = true
                else if (buttonGroup.checkedButton)
                    buttonGroup.checkedButton.checked = false
            }

            function setTabState(tab, isCurrent)
            {
                if (isCurrent)
                    d.setCurrentTab(tab)
                else if (d.currentTab === tab)
                    d.setCurrentTab(d.previousTab)
            }

            function hasRightmostResizer(tab)
            {
                return tab === RightPanel.Tab.resources
                    || tab === RightPanel.Tab.analytics
            }
        }
    }
}
