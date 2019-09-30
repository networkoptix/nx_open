import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

Item
{
    TabBar
    {
        id: tabBar

        width: parent.width

        CompactTabButton
        {
            id: notificationsButton
            icon.source: "qrc:/skin/events/tabs/notifications.png"
            text: qsTr("Notifications")
        }

        CompactTabButton
        {
            id: motionButton
            icon.source: "qrc:/skin/events/tabs/motion.png"
            text: qsTr("Motion")
        }

        CompactTabButton
        {
            id: bookmarksButton
            icon.source: "qrc:/skin/events/tabs/bookmarks.png"
            text: qsTr("Bookmarks")
        }

        CompactTabButton
        {
            id: eventsButton
            icon.source: "qrc:/skin/events/tabs/events.png"
            text: qsTr("Events")
        }

        CompactTabButton
        {
            id: analyticsButton
            icon.source: "qrc:/skin/events/tabs/analytics.png"
            text: qsTr("Analytics")
        }
    }

    Container
    {
        id: tabView

        width: parent.width
        anchors.top: tabBar.bottom
        anchors.bottom: parent.bottom
        currentIndex: tabBar.currentIndex

        contentItem: Item {}

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Item
            {
                id: notificationsItem

                EventRibbon
                {
                    anchors.fill: parent
                    model: RightPanelModel
                    {
                        context: workbenchContext
                        type: RightPanel.Tab.notifications
                    }
                }
            }
        }

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Item
            {
                id: motionItem

                EventRibbon
                {
                    brief: true
                    anchors.fill: parent
                    model: RightPanelModel
                    {
                        context: workbenchContext
                        type: RightPanel.Tab.motion
                    }
                }
            }
        }

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Item
            {
                id: bookmarksItem

                EventRibbon
                {
                    anchors.fill: parent
                    model: RightPanelModel
                    {
                        context: workbenchContext
                        type: RightPanel.Tab.bookmarks
                    }
                }
            }
        }

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Item
            {
                id: eventsItem

                EventRibbon
                {
                    anchors.fill: parent
                    model: RightPanelModel
                    {
                        context: workbenchContext
                        type: RightPanel.Tab.events
                    }
                }
            }
        }

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Item
            {
                id: analyticsItem

                EventRibbon
                {
                    anchors.fill: parent
                    model: RightPanelModel
                    {
                        context: workbenchContext
                        type: RightPanel.Tab.analytics
                    }
                }
            }
        }
    }
}
