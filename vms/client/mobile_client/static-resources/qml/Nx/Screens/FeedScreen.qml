// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Core
import Nx.Items
import Nx.Mobile.Controls
import Nx.Ui

import nx.vms.client.mobile

import "private/FeedScreen"

AdaptiveScreen
{
    id: feedScreen

    objectName: "feedScreen"

    required property FeedStateProvider feedState

    title: feed.title

    customBackHandler: contentItem === notificationDetailsItem
        ? () => d.closeDetails()
        : null

    // The right (Details) panel mirrors `feed.selectedNotification`. Clear the selection whenever
    // AdaptiveScreen closes the panel (close button, auto-close, or cross-close), otherwise
    // `rightPanel.item` stays bound to the same `notificationDetailsItem` and a repeated tap on
    // the same card produces no `onItemChanged` — the panel would not reopen.
    onPanelClosed: (panel) =>
    {
        if (panel === rightPanel)
            feed.selectedNotification = null
    }

    contentItem: feed

    leftPanel
    {
        title: qsTr("Period")
        color: ColorTheme.colors.dark5
        iconSource: "image://skin/24x24/Outline/filter_list.svg?primary=dark1"
        interactive: true
        visible: false
        item: LayoutController.isTabletLayout && d.canFilter ? leftPanelItem : null
    }

    leftPanelButtonIndicator.visible: feed.filtered

    rightPanel
    {
        title: notificationDetailsItem.title
        color: ColorTheme.colors.dark5
        interactive: true
        item: LayoutController.isTabletLayout && feed.selectedNotification
            ? notificationDetailsItem
            : null

        onItemChanged:
        {
            if (rightPanel.item)
                feedScreen.showPanel(rightPanel)
        }
    }

    customLeftControl: ToolBarButton
    {
        id: backButton

        anchors.centerIn: parent
        visible: feedScreen.contentItem === notificationDetailsItem
        icon.source: "image://skin/24x24/Outline/arrow_back.svg?primary=light4"

        onClicked: d.closeDetails()
    }

    customRightControl: IconButton
    {
        id: filterButton

        icon.source: "image://skin/24x24/Outline/filter_list.svg?primary=light4"
        icon.width: 24
        icon.height: 24

        indicator.visible: feed.filtered

        visible: !LayoutController.isTabletLayout && d.canFilter && feedScreen.contentItem === feed

        onClicked: feed.openFilterMenu()
    }

    Feed
    {
        id: feed

        feedState: feedScreen.feedState

        onSelectedNotificationChanged:
        {
            if (!selectedNotification || LayoutController.isTabletLayout)
                return

            feedScreen.contentItem = notificationDetailsItem
        }
    }

    Item
    {
        id: leftPanelItem

        ButtonGroup { id: filtersButtonGroup }

        Column
        {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 20

            spacing: 4

            component FilterButton: StyledRadioButton
            {
                height: 56
                width: parent.width
                backgroundRadius: 8
                indicator: null
            }

            FilterButton
            {
                text: qsTr("Unviewed")
                checked: feed.filterModel.filter === PushNotificationFilterModel.Unviewed
                onToggled: feed.filterModel.filter = PushNotificationFilterModel.Unviewed

                ButtonGroup.group: filtersButtonGroup
            }

            FilterButton
            {
                text: qsTr("All")
                checked: feed.filterModel.filter === PushNotificationFilterModel.All
                onToggled: feed.filterModel.filter = PushNotificationFilterModel.All

                ButtonGroup.group: filtersButtonGroup
            }
        }
    }

    NotificationDetailsItem
    {
        id: notificationDetailsItem

        notification: feed.selectedNotification

        onBackClicked: d.closeDetails()
    }

    Connections
    {
        target: LayoutController

        function onFullscreenChanged()
        {
            if (!feedScreen.isActive)
                return

            if (!LayoutController.isTabletLayout)
                return

            if (LayoutController.fullscreen)
            {
                feedScreen.contentItem = notificationDetailsItem
            }
            else if (feedScreen.contentItem === notificationDetailsItem)
            {
                feedScreen.contentItem = feed
                feedScreen.showPanel(rightPanel)
            }
        }

        function onIsPortraitChanged()
        {
            if (!feedScreen.isActive)
                return

            if (feedScreen.contentItem === feed) //< Feed list fits any orientation.
            {
                feed.selectedNotification = null
                return
            }

            if (LayoutController.isMobile)
                return

            if (LayoutController.isPortrait)
                return

            if (LayoutController.fullscreen)
                return

            feedScreen.contentItem = feed
            if (rightPanel.item)
                feedScreen.showPanel(rightPanel)
        }
    }

    QtObject
    {
        id: d

        property bool canFilter: feedState.hasOsPermission
            && feedState.notificationsEnabled
            && (feedState.count !== 0 || feed.searching || feed.filtered)

        function closeDetails()
        {
            if (LayoutController.fullscreen)
                LayoutController.exitFullscreen()

            if (LayoutController.isTabletLayout)
                return

            feedScreen.contentItem = feed
            feed.selectedNotification = null
        }
    }
}
