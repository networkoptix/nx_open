// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

TabControl
{
    id: tabs

    Tab
    {
        id: notificationsTab

        button: CompactTabButton
        {
            leftPadding: 0
            rightPadding: 0
            focusFrameEnabled: false
            icon.source: "qrc:/skin/events/tabs/notifications.png"
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
                    context: workbenchContext
                    type: { return RightPanelModel.Type.notifications }
                }
            }
        }
    }

    Tab
    {
        id: motionTab

        button: CompactTabButton
        {
            leftPadding: 0
            rightPadding: 0
            focusFrameEnabled: false
            icon.source: "qrc:/skin/events/tabs/motion.png"
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
                    context: workbenchContext
                    type: { return RightPanelModel.Type.motion }
                }
            }
        }
    }

    Tab
    {
        id: bookmarksTab

        button: CompactTabButton
        {
            leftPadding: 0
            rightPadding: 0
            focusFrameEnabled: false
            icon.source: "qrc:/skin/events/tabs/bookmarks.png"
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
                    context: workbenchContext
                    type: { return RightPanelModel.Type.bookmarks }
                }
            }
        }
    }

    Tab
    {
        id: eventsTab

        button: CompactTabButton
        {
            leftPadding: 0
            rightPadding: 0
            focusFrameEnabled: false
            icon.source: "qrc:/skin/events/tabs/events.png"
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
                    context: workbenchContext
                    type: { return RightPanelModel.Type.events }
                }
            }
        }
    }

    Tab
    {
        id: analyticsTab

        button: CompactTabButton
        {
            leftPadding: 0
            rightPadding: 0
            focusFrameEnabled: false
            icon.source: "qrc:/skin/events/tabs/analytics.png"
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
                    context: workbenchContext
                    type: { return RightPanelModel.Type.analytics }
                }
            }
        }
    }
}
