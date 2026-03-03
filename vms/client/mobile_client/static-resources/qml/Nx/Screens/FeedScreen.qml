// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

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

    rightPanel
    {
        title: qsTr("Details")
        color: ColorTheme.colors.dark5
        interactive: true
        item: LayoutController.isTabletLayout && feed.selectedNotification
            ? notificationDetailsItem
            : null

        onItemChanged: rightPanel.visible = !!rightPanel.item
        onCloseButtonClicked: feed.selectedNotification = null
    }

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

    Item
    {
        id: notificationDetailsItem

        MouseArea
        {
            anchors.fill: parent

            onClicked:
            {
                const url = feed.selectedNotification?.url
                if (url)
                    windowContext.uriHandler.handleUrl(url)
            }
        }

        Image //< TODO: It must be player here if there is something to view.
        {
            id: notificationDetailImage

            width: parent.width
            height: 180

            visible: status != Image.Null
            fillMode: Image.PreserveAspectCrop

            source: feed.selectedNotification?.image ?? ""

            Rectangle
            {
                anchors.fill: parent

                visible: notificationDetailImage.status != Image.Ready
                color: ColorTheme.colors.dark8

                Text
                {
                    anchors.centerIn: parent

                    visible: notificationDetailImage.status === Image.Error
                    text: qsTr("No data")
                    font.pixelSize: 12
                    color: ColorTheme.colors.dark17
                }
            }
        }

        Flickable
        {
            id: notificationDetailsFlickable

            anchors.fill: parent
            anchors.margins: 20
            anchors.topMargin: 20 + (notificationDetailImage.visible ? notificationDetailImage.height : 0)

            contentHeight: notificationDetailsColumn.implicitHeight
            clip: true

            Connections
            {
                target: feed

                function onSelectedNotificationChanged()
                {
                    notificationDetailsFlickable.contentY = 0
                }
            }

            ColumnLayout
            {
                id: notificationDetailsColumn

                width: parent.width
                spacing: 16

                Text
                {
                    Layout.fillWidth: true

                    text: feed.selectedNotification?.title ?? ""
                    visible: !!text
                    font.pixelSize: 18
                    color: ColorTheme.colors.light4
                    wrapMode: Text.WordWrap
                }

                Text
                {
                    Layout.fillWidth: true

                    text: feed.selectedNotification?.description ?? ""
                    visible: !!text
                    font.pixelSize: 16
                    color: ColorTheme.colors.light10
                    wrapMode: Text.WordWrap
                }

                Text
                {
                    Layout.fillWidth: true

                    font.pixelSize: 14
                    font.capitalization: Font.AllUppercase
                    font.weight: Font.Medium

                    color: ColorTheme.colors.light16
                    text: feed.selectedNotification?.time ?? ""
                }
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
