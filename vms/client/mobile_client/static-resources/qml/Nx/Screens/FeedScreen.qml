// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Core.Ui
import Nx.Mobile.Controls
import Nx.Controls
import Nx.Items
import Nx.Ui

import nx.vms.client.core
import nx.vms.client.mobile

import "private/FeedScreen"

Page
{
    id: feedScreen

    required property FeedStateProvider feedState

    property color highlightColor: ColorTheme.colors.yellow_d1

    readonly property bool searching: !!search.text
    readonly property bool filtered: filterModel.filter !== PushNotificationFilterModel.All
    readonly property bool empty: notifications.count === 0

    objectName: "feedScreen"

    title: qsTr("Feed")

    topPadding: 8
    leftPadding: 20
    rightPadding: 20

    onLeftButtonClicked: Workflow.popCurrentScreen()

    rightControl: IconButton
    {
        id: filterButton

        anchors.centerIn: parent

        icon.source: "image://skin/24x24/Outline/filter_list.svg?primary=light4"
        icon.width: 24
        icon.height: 24

        onClicked: filterMenu.open()
    }

    states:
    [
        State
        {
            when: !feedState.hasOsPermission

            PropertyChanges
            {
                search.opacity: 0
                notifications.opacity: 0
                filterButton.opacity: 0

                placeholder.text: qsTr("Notifications Off")
                placeholder.description: qsTr("Notifications are currently disabled for this app. "
                    + "To enable notifications use your phone's settings")
                placeholder.imageSource:
                    "image://skin/64x64/Outline/notification_off.svg?primary=light10"
            }
        },
        State
        {
            when: !feedState.notificationsEnabled

            PropertyChanges
            {
                search.opacity: 0
                notifications.opacity: 0
                filterButton.opacity: 0

                placeholder.text: qsTr("Notifications Off")
                placeholder.description: qsTr("You disabled push notifications for this site. To "
                    + "enable them, go to the Settings page.")
                placeholder.imageSource:
                    "image://skin/64x64/Outline/notification_off.svg?primary=light10"

                placeholder.buttonText: qsTr("Settings")
                placeholder.onButtonClicked: Workflow.openSettingsScreen()
            }
        },
        State
        {
            when: empty && !searching && !filtered

            PropertyChanges
            {
                search.opacity: 0

                placeholder.text: qsTr("No Notifications")
                placeholder.description: qsTr("No push notifications were found.")
                placeholder.imageSource:
                    "image://skin/64x64/Outline/notification.svg?primary=light10"
            }
        },
        State
        {
            when: empty && searching

            PropertyChanges
            {
                placeholder.text: qsTr("Nothing found")
                placeholder.description: qsTr("Try changing the search parameters")
                placeholder.imageSource: ""
            }
        },
        State
        {
            when: empty && !searching && filtered

            PropertyChanges
            {
                search.opacity: 0

                placeholder.text: qsTr("No New Notifications")
                placeholder.description: qsTr("No new push notifications were found, but you can "
                    + "view your full notification history.")
                placeholder.imageSource:
                    "image://skin/64x64/Outline/notification.svg?primary=light10"

                placeholder.buttonText: qsTr("View All")
                placeholder.onButtonClicked: filterModel.filter = PushNotificationFilterModel.All
            }
        }
    ]

    component Button: AbstractButton
    {
        id: control

        implicitWidth: 100
        implicitHeight: 100

        icon.width: 20
        icon.height: 20
        icon.color: ColorTheme.colors.light1
        focusPolicy: Qt.NoFocus

        property alias backgroundColor: background.color

        background: Rectangle
        {
            id: background
            color: ColorTheme.colors.brand
            radius: 10
        }

        contentItem: Item
        {
            Column
            {
                anchors.centerIn: parent
                spacing: 8

                ColoredImage
                {
                    anchors.horizontalCenter: parent.horizontalCenter
                    sourceSize: Qt.size(icon.width, icon.height)
                    sourcePath: icon.source
                    primaryColor: icon.color
                }

                Text
                {
                    text: control.text
                    font.pixelSize: 16
                    font.weight: Font.Medium
                    color: icon.color
                }
            }
        }
    }

    ColumnLayout
    {
        anchors.fill: parent
        spacing: 16

        SearchEdit
        {
            id: search

            Layout.fillWidth: true
            Layout.preferredHeight: 36

            readonly property var regExp:
                new RegExp(`(${NxGlobals.makeSearchRegExpNoAnchors(text)})`, 'i')

            property string text: ""

            PropertyUpdateFilter on text
            {
                source: search.displayText
                minimumIntervalMs: 250
            }
        }

        ListView
        {
            id: notifications

            Layout.fillWidth: true
            Layout.fillHeight: true

            clip: true
            width: parent.width
            spacing: 8

            focus: true
            focusPolicy: Qt.WheelFocus

            model: PushNotificationFilterModel
            {
                id: filterModel

                sourceModel: PushNotificationModel
                {
                    id: sourceModel

                    function update() { sourceModel.notifications = feedState.notifications }

                    Component.onCompleted: update()
                }

                filterRegularExpression: search.regExp
            }

            delegate: SwipeControl
            {
                width: notifications.width

                revealDistance: 100
                activationDistance: 100

                contentItem: Notification
                {
                    width: parent.width

                    title: highlightMatchingText(model.title)
                    description: highlightMatchingText(model.description)
                    source: feedState.cloudSystemIds.length > 1 ? model.source : ""
                    image: model.image
                    time: model.time
                    viewed: model.viewed
                    url: model.url

                    onClicked:
                    {
                        if (model.url)
                            windowContext.uriHandler.handleUrl(model.url)

                        feedState.setViewed(model.id, true)
                        feedState.update()
                            
                        model.viewed = true
                    }
                }

                leftItem: Button
                {
                    text: qsTr("Unviewed")
                    icon.source: "image://skin/20x20/Solid/eye_off.svg"
                    backgroundColor: ColorTheme.colors.light16

                    onClicked:
                    {
                        feedState.setViewed(model.id, false)
                        feedState.update()
                        
                        model.viewed = false
                    }
                }

                rightItem: Button
                {
                    text: qsTr("Viewed")
                    icon.source: "image://skin/20x20/Solid/eye.svg"
                    backgroundColor: ColorTheme.colors.brand

                    onClicked:
                    {
                        feedState.setViewed(model.id, true)
                        feedState.update()
                        
                        model.viewed = true
                    }
                }
            }
        }
    }

    Menu
    {
        id: filterMenu

        parent: filterButton
        y: filterButton.height
        width: 240

        MenuItem
        {
            text: qsTr("Unviewed")
            checkable: true
            checked: filterModel.filter === PushNotificationFilterModel.Unviewed
            onToggled: filterModel.filter = PushNotificationFilterModel.Unviewed
            autoExclusive: true
        }

        MenuItem
        {
            text: qsTr("All")
            checkable: true
            checked: filterModel.filter === PushNotificationFilterModel.All
            onToggled: filterModel.filter = PushNotificationFilterModel.All
            autoExclusive: true
        }
    }

    Placeholder
    {
        id: placeholder

        anchors.centerIn: parent
        visible: !!text
    }

    ViewUpdateWatcher
    {
        id: refreshWatcher

        view: notifications

        onUpdateRequested:
        {
            sourceModel.update()
            feedState.update()
        }
    }

    RefreshIndicator
    {
        anchors.horizontalCenter: parent.horizontalCenter
        progress: refreshWatcher.refreshProgress
        y: notifications.y + refreshWatcher.overshoot * progress - height - notifications.spacing * 2
    }

    function highlightMatchingText(text)
    {
        return NxGlobals.highlightMatch(text, search.regExp, feedScreen.highlightColor)
    }
}
