import QtQuick 2.6
import QtQuick.Controls 2.4

import Nx.Controls 1.0

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

        // TODO: #vkutin This is a stub that will be replaced with actual tabs content.

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Rectangle
            {
                id: notificationsItem
                color: "grey"
            }
        }

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Rectangle
            {
                id: motionsItem
                color: "green"
            }
        }

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Rectangle
            {
                id: bookmarksItem
                color: "cyan"
            }
        }

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Rectangle
            {
                id: eventsItem
                color: "yellow"
            }
        }

        Fader
        {
            anchors.fill: parent
            opaque: tabView.currentItem === this

            contentItem: Rectangle
            {
                id: analyticsItem
                color: "red"
            }
        }
    }
}
