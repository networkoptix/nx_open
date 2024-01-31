// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

import nx.vms.client.desktop

TabControl
{
    id: tabs

    Tab
    {
        id: notificationsTab

        component RightPanelTabButton: CompactTabButton
        {
            leftPadding: 0
            rightPadding: 0
            focusFrameEnabled: false
            icon.width: 20
            icon.height: 20
            font.capitalization: Font.AllUppercase
        }

        button: RightPanelTabButton
        {
            icon.source: "image://svg/skin/events/tabs/notifications.svg?primary=white"
            text: qsTr("Notifications")
        }

        Item
        {
            id: notificationsItem

            EventRibbon
            {
                anchors.fill: parent
                model: RightPanelModel
                {
                    context: windowContext
                    type: { return RightPanelModel.Type.notifications }
                }
            }
        }
    }

    Tab
    {
        id: motionTab

        button: RightPanelTabButton
        {
            icon.source: "image://svg/skin/events/tabs/motion.svg?primary=white"
            text: qsTr("Motion")
        }

        Item
        {
            id: motionItem

            EventRibbon
            {
                brief: true
                anchors.fill: parent
                model: RightPanelModel
                {
                    context: windowContext
                    type: { return RightPanelModel.Type.motion }
                }
            }
        }
    }

    Tab
    {
        id: bookmarksTab

        button: RightPanelTabButton
        {
            icon.source: "image://svg/skin/events/tabs/bookmarks.svg?primary=white"
            text: qsTr("Bookmarks")
        }

        Item
        {
            id: bookmarksItem

            EventRibbon
            {
                anchors.fill: parent
                model: RightPanelModel
                {
                    context: windowContext
                    type: { return RightPanelModel.Type.bookmarks }
                }
            }
        }
    }

    Tab
    {
        id: eventsTab

        button: RightPanelTabButton
        {
            icon.source: "image://svg/skin/events/tabs/events.svg?primary=white"
            text: qsTr("Events")
        }

        Item
        {
            id: eventsItem

            EventRibbon
            {
                anchors.fill: parent
                model: RightPanelModel
                {
                    context: windowContext
                    type: { return RightPanelModel.Type.events }
                }
            }
        }
    }

    Tab
    {
        id: analyticsTab

        button: RightPanelTabButton
        {
            icon.source: "image://svg/skin/events/tabs/analytics.svg?primary=white"
            text: qsTr("Analytics")
        }

        Item
        {
            id: analyticsItem

            EventRibbon
            {
                anchors.fill: parent
                model: RightPanelModel
                {
                    context: windowContext
                    type: { return RightPanelModel.Type.analytics }
                }
            }
        }
    }
}
