// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.RightPanel

import nx.vms.client.desktop

import ".."

Item
{
    id: notificationsPanel

    readonly property alias model: notificationsModel
    property alias active: notificationsModel.active

    readonly property alias unreadCount: notificationsModel.unreadCount
    readonly property alias unreadColor: notificationsModel.unreadColor

    EventRibbon
    {
        id: notificationsRibbon

        anchors.fill: parent

        model: RightPanelModel
        {
            id: notificationsModel

            context: WindowContextAware.context
            type: { return RightPanelModel.Type.notifications }

            unreadTracking: true
        }

        placeholder
        {
            icon: "image://skin/64x64/Outline/notification.svg"

            title: qsTr("No new notifications")

            action: Action
            {
                text: qsTr("Notifications Settings")

                onTriggered:
                {
                    WindowContextAware.triggerAction(
                        GlobalActions.PreferencesNotificationTabAction)
                }
            }
        }

        CounterBlock
        {
            parent: notificationsRibbon.headerItem
            width: notificationsRibbon.width - notificationsRibbon.ScrollBar.vertical.width
            displayedItemsText: notificationsRibbon.model.itemCountText
            showThumbnailsButton: false
            leftInset: 1
        }
    }
}
