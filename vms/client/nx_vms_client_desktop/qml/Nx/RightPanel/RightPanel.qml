// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Controls

import nx.vms.client.desktop

import "private"
import "private/panels"

Item
{
    id: rightPanel

    property var scene

    TabControl
    {
        id: tabs

        anchors.fill: parent

        tabBar
        {
            x: 0
            width: parent.width
            height: 32
            spacing: 0
        }

        Tab
        {
            id: notificationsTab

            button: RightPanelTabButton
            {
                id: notificationButton

                icon.source: animatedIcon.active
                    ? undefined
                    : "image://skin/events/tabs/notifications.svg"

                icon.name: "RightPanel.Tabs.Notifications"
                text: qsTr("Notifications")

                CallNotificationIcon
                {
                    id: animatedIcon

                    parent: notificationButton.iconImage
                    anchors.fill: parent

                    active: alarmManager.ringing

                    CallAlarmManager
                    {
                        id: alarmManager

                        context: WindowContextAware.context
                        active: d.currentTab !== notificationsTab
                    }
                }

                UnreadNotificationCounter
                {
                    id: unreadCounter

                    count: notificationsPanel.unreadCount
                    color: notificationsPanel.unreadColor

                    parent: notificationButton.iconImage
                    anchors.top: parent.top
                    anchors.right: parent.right
                    z: 1
                }
            }

            NotificationsPanel
            {
                id: notificationsPanel
                objectName: "NotificationsPanel"

                onActiveChanged:
                    d.setTabState(notificationsTab, /*isCurrent*/ active)
            }
        }

        Tab
        {
            id: motionTab

            visible: motionPanel.model.isAllowed

            button: RightPanelTabButton
            {
                icon.source: "image://skin/events/tabs/motion.svg"
                icon.name: "RightPanel.Tabs.Motion"
                text: qsTr("Motion")
            }

            MotionPanel
            {
                id: motionPanel
                objectName: "MotionPanel"

                onActiveChanged:
                    d.setTabState(motionTab, /*isCurrent*/ active)
            }
        }

        Tab
        {
            id: bookmarksTab

            visible: bookmarksPanel.model.isAllowed

            button: RightPanelTabButton
            {
                icon.source: "image://skin/events/tabs/bookmarks.svg"
                icon.name: "RightPanel.Tabs.Bookmarks"
                text: qsTr("Bookmarks")
            }

            BookmarksPanel
            {
                id: bookmarksPanel
                objectName: "BookmarksPanel"

                onActiveChanged:
                    d.setTabState(bookmarksTab, /*isCurrent*/ active)
            }
        }

        Tab
        {
            id: eventsTab

            visible: eventsPanel.model.isAllowed

            button: RightPanelTabButton
            {
                icon.source: "image://skin/events/tabs/events.svg"
                icon.name: "RightPanel.Tabs.Events"
                text: qsTr("Events")
            }

            EventsPanel
            {
                id: eventsPanel
                objectName: "EventsPanel"

                onActiveChanged:
                    d.setTabState(eventsTab, /*isCurrent*/ active)
            }
        }

        Tab
        {
            id: objectsTab

            visible: objectsPanel.model.isAllowed

            button: RightPanelTabButton
            {
                icon.source: "image://skin/events/tabs/analytics.svg"
                icon.name: "RightPanel.Tabs.Objects"
                text: qsTr("Objects")
            }

            AnalyticsPanel
            {
                id: objectsPanel
                objectName: "ObjectsPanel"

                onActiveChanged:
                    d.setTabState(objectsTab, /*isCurrent*/ active)
            }
        }
    }

    NxObject
    {
        id: d

        property Tab currentTab: notificationsTab
        property Tab previousTab: null

        property bool updating: false

        function setCurrentTab(tab)
        {
            if (!tab || !tab.visible || tab === d.currentTab || d.updating)
                return

            d.updating = true
            d.previousTab = d.currentTab
            d.currentTab = tab

            for (let otherTab of tabs.tabs)
            {
                if (otherTab.page.hasOwnProperty("active"))
                    otherTab.page.active = otherTab === d.currentTab
            }

            tabs.selectTab(d.currentTab)
            d.updating = false
        }

        function setTabState(tab, isCurrent)
        {
            if (isCurrent)
                d.setCurrentTab(tab)
            else if (d.currentTab === tab)
                d.setCurrentTab(d.previousTab)
        }

        Connections
        {
            target: tabs

            // We need this to handle tab changes caused by user interaction,
            // to update d.currentTab and d.previousTab and every page's active state.

            function onCurrentTabChanged()
            {
                d.setCurrentTab(tabs.currentTab)
            }
        }
    }
}
