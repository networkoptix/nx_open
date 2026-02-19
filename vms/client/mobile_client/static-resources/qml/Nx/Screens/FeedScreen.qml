// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Items
import Nx.Ui

import nx.vms.client.mobile

import "private/FeedScreen"

AdaptiveScreen
{
    id: feedScreen

    objectName: "feedScreen"

    required property FeedStateProvider feedState

    title: qsTr("Feed")

    contentItem: Feed
    {
        id: feed

        feedState: feedScreen.feedState
    }

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

    customRightControl: IconButton
    {
        id: filterButton

        icon.source: "image://skin/24x24/Outline/filter_list.svg?primary=light4"
        icon.width: 24
        icon.height: 24

        indicator.visible: feed.filtered

        visible: !LayoutController.isTabletLayout && d.canFilter

        onClicked: feed.openFilterMenu()
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

    QtObject
    {
        id: d

        property bool canFilter: feedState.hasOsPermission
            && feedState.notificationsEnabled
            && (!feed.empty || feed.searching || feed.filtered)
    }
}
