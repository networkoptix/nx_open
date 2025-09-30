// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items
import Nx.Ui

import nx.vms.client.mobile

Page
{
    id: feedScreen

    objectName: "feedScreen"

    title: qsTr("Feed")

    leftButtonIcon.source: ""

    component Notification: Control
    {
        id: control

        objectName: "notification"

        property alias title: title.text
        property alias description: description.text
        property alias time: time.text
        property alias image: image.source
        property string source: ""
        property bool read: false
        property bool expanded: false

        readonly property int kTitleRightPadding: 24

        implicitWidth: 320
        padding: 20

        background: Rectangle
        {
            color: read ? ColorTheme.colors.dark7 : ColorTheme.colors.dark9
            radius: 10

            Rectangle
            {
                id: indicator

                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 28
                anchors.rightMargin: 20

                visible: !read
                width: 8
                height: 8
                radius: width / 2
                color: ColorTheme.colors.brand
            }
        }

        contentItem: ColumnLayout
        {
            spacing: 16

            RowLayout
            {
                Layout.rightMargin: kTitleRightPadding

                visible: !!source
                spacing: 8

                ColoredImage
                {
                    sourcePath: "image://skin/20x20/Solid/site_local.svg"
                    sourceSize: Qt.size(20, 20)
                    primaryColor: ColorTheme.colors.light10
                }

                Text
                {
                    Layout.fillWidth: true

                    text: source
                    visible: !!text
                    font.pixelSize: 16
                    color: ColorTheme.colors.light10

                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }

            Text
            {
                id: title

                Layout.fillWidth: true
                Layout.rightMargin: source ? undefined : kTitleRightPadding

                font.pixelSize: 18
                font.weight: Font.Medium
                color: ColorTheme.colors.light4

                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Image
            {
                id: image

                Layout.fillWidth: true
                Layout.preferredHeight: 180

                visible: status === Image.Ready

                fillMode: Image.PreserveAspectCrop

                layer.enabled: true
                layer.effect: OpacityMask
                {
                    maskSource: Rectangle
                    {
                        width: image.paintedWidth
                        height: image.paintedHeight
                        radius: 4
                    }
                }
            }

            Text
            {
                id: description

                Layout.fillWidth: true

                color: ColorTheme.colors.light10
                font.pixelSize: 16

                wrapMode: Text.Wrap
                maximumLineCount: expanded ? undefined : 2
                elide: Text.ElideRight
            }

            RowLayout
            {
                spacing: 4

                Text
                {
                    id: time

                    Layout.fillWidth: true

                    font.pixelSize: 14
                    font.capitalization: Font.AllUppercase
                    font.weight: Font.Medium
                    color: ColorTheme.colors.light16
                }

                Text
                {
                    visible: description.truncated

                    text: qsTr("Show more")
                    font.pixelSize: 16
                    font.capitalization: Font.AllUppercase
                    font.weight: Font.Medium
                    color: ColorTheme.colors.light16

                    TapHandler
                    {
                        margin: 40
                        onTapped: expanded = true
                    }
                }

                ColoredImage
                {
                    visible: description.truncated

                    sourceSize: Qt.size(24, 24)
                    sourcePath: "image://skin/24x24/Outline/arrow_down_2px.svg"
                    primaryColor: ColorTheme.colors.light16
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

        delegate: Notification
        {
            width: listView.width

            title: model.title
            description: model.description
            image: model.image
            time: model.time
            read: model.read
        }
    }
}
