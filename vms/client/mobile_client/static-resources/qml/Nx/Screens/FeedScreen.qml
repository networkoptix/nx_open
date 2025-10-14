// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls
import Nx.Mobile.Controls
import Nx.Controls
import Nx.Items

import nx.vms.client.mobile

import "private/FeedScreen"

Page
{
    id: feedScreen

    objectName: "feedScreen"

    title: qsTr("Feed")

    leftButtonIcon.source: ""

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

    Placeholder
    {
        anchors.centerIn: parent

        visible: listView.count === 0
        text: qsTr("No Notifications")
        imageSource: "image://skin/64x64/Outline/notification.svg?primary=light10"
        description: qsTr("No push notifications were found.")
    }

    ListView
    {
        id: listView

        anchors.fill: parent
        anchors.leftMargin: 20
        anchors.rightMargin: 20

        spacing: 16
        visible: count > 0
        model: PushNotificationModel { }

        delegate: SwipeControl
        {
            width: listView.width

            revealDistance: 100
            activationDistance: 100

            contentItem: Notification
            {
                width: parent.width

                title: model.title
                description: model.description
                image: model.image
                time: model.time
                read: model.read

                onClicked: model.read = true
            }

            leftItem: Button
            {
                text: qsTr("Unviewed")
                icon.source: "image://skin/20x20/Solid/eye_off.svg"
                backgroundColor: ColorTheme.colors.light16

                onClicked: model.read = false
            }

            rightItem: Button
            {
                text: qsTr("Viewed")
                icon.source: "image://skin/20x20/Solid/eye.svg"
                backgroundColor: ColorTheme.colors.brand

                onClicked: model.read = true
            }
        }
    }
}
