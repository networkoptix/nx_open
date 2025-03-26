// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls
import Nx.Items
import Nx.Ui

import "private"

Page
{
    id: cloudScreen
    objectName: "cloudSummaryScreen"

    readonly property string fullName: cloudUserProfileWatcher.fullName
    readonly property var channelPartnerList: cloudUserProfileWatcher.channelPartnerList

    padding: 0

    onLeftButtonClicked: Workflow.popCurrentScreen()

    title: qsTr("%1 Account", "%1 is the short cloud name (like 'Cloud')")
        .arg(applicationInfo.shortCloudName())

    Rectangle
    {
        id: userNameContainer

        y: 8
        width: parent.width
        height: childrenRect.height + 20
        color: ColorTheme.colors.dark6

        ColumnLayout
        {
            width: parent.width
            spacing: 0

            Image
            {
                Layout.topMargin: 20
                Layout.alignment: Qt.AlignHCenter

                Layout.preferredWidth: 64
                Layout.preferredHeight: 64

                fillMode: Image.PreserveAspectFit

                source: cloudUserProfileWatcher.avatarUrl
            }

            Text
            {
                id: fullNameLabel
                Layout.topMargin: 12
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20

                horizontalAlignment: Text.AlignHCenter
                text: cloudScreen.fullName
                elide: Text.ElideRight
                font.pixelSize: 18
                font.weight: Font.Medium
                color: ColorTheme.colors.light4
            }

            Text
            {
                Layout.topMargin: 6
                Layout.fillWidth: true
                Layout.leftMargin: 20
                Layout.rightMargin: 20

                horizontalAlignment: Text.AlignHCenter
                text: cloudStatusWatcher.cloudLogin
                elide: Text.ElideRight
                font.pixelSize: 14
                font.weight: Font.Normal
                color: ColorTheme.colors.light14
            }

            GridLayout
            {
                Layout.topMargin: 16
                Layout.alignment: Qt.AlignHCenter
                Layout.leftMargin: 20
                Layout.rightMargin: 20

                columns: cloudScreen.width > 600 ? 2 : 1
                columnSpacing: 8
                rowSpacing: 8
                uniformCellWidths: true

                Button
                {
                    id: openCloudButton

                    Layout.fillWidth: true

                    text: qsTr("Open %1", "%1 is the short cloud name (like 'Cloud')").arg(
                        applicationInfo.cloudName())

                    padding: 0
                    leftPadding: 0
                    rightPadding: 0

                    onClicked:
                    {
                        // Forcing active focus to prevent keyboard capture form the QML side.
                        // Otherwise embedded browser can't show keyboard for input fields.
                        openCloudButton.forceActiveFocus()
                        openExternalLink(cloudUrlHelper.mainUrl())
                    }
                }

                Button
                {
                    id: logoutButton

                    Layout.fillWidth: true

                    text: qsTr("Log out")

                    padding: 0
                    leftPadding: 0
                    rightPadding: 0

                    onClicked:
                    {
                        cloudStatusWatcher.resetAuthData();
                        Workflow.popCurrentScreen()
                    }
                }
            }
        }
    }

    Item
    {
        id: channelPartnerContainer

        visible: channelPartnerList && channelPartnerList.length

        anchors.top: userNameContainer.bottom
        anchors.topMargin: 5

        width: parent.width

        Text
        {
            id: channelPartnerLabel

            anchors.top: parent.top
            anchors.topMargin: 14
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.right: parent.right
            anchors.rightMargin: 20
            elide: Text.ElideRight

            text: qsTr("Channel Partner")

            color: ColorTheme.colors.light14
            font.pixelSize: 14
            font.weight: Font.Normal
        }

        Rectangle
        {
            id: channelPartnerItem

            anchors.top: channelPartnerLabel.bottom
            anchors.topMargin: 5

            width: parent.width
            height: 56
            color: ColorTheme.colors.dark6

            Text
            {
                anchors.verticalCenter: parent.verticalCenter
                text:
                {
                    if (!channelPartnerList || !channelPartnerList.length)
                        return "?"

                    for (let partner of channelPartnerList)
                    {
                        if (partner.id == cloudUserProfileWatcher.channelPartner)
                            return partner.name
                    }

                    return channelPartnerList[0].name
                }
                font.pixelSize: 16
                font.weight: Font.Normal
                color: ColorTheme.colors.light4
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.right: channelPartnerSelectIcon.left
                elide: Text.ElideRight
            }

            ColoredImage
            {
                id: channelPartnerSelectIcon

                visible: channelPartnerList && channelPartnerList.length > 1
                anchors.verticalCenter: parent.verticalCenter
                anchors.right: parent.right
                anchors.rightMargin: 20

                sourceSize: Qt.size(24, 24)
                primaryColor: ColorTheme.colors.dark14
                sourcePath: "image://skin/20x20/Outline/arrow_right.svg"
            }

            TapHandler
            {
                enabled: channelPartnerList && channelPartnerList.length > 1
                onSingleTapped:
                {
                    Workflow.openChannelPartnerScreen(cloudUserProfileWatcher)
                }
            }
        }
    }
}
