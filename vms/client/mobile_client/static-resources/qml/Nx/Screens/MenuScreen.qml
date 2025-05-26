// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls
import Nx.Ui

import nx.vms.client.core

Page
{
    id: menuScreen

    objectName: "menuScreen"

    title: ""
    leftButtonIcon.source: ""

    ListView
    {
        id: buttonsView

        x: 16
        y: 16
        width: parent.width - x * 2
        height: parent.height - y * 2
        spacing: 6

        verticalLayoutDirection: ListView.BottomToTop

        model: [
            {
                "iconSource": "image://skin/24x24/Outline/disconnect.svg",
                "text": windowContext.sessionManager.systemName,
                "initialize": (item) => item.customAreaComponent = disconnectButtonCustomArea,
                "action": () => windowContext.sessionManager.stopSessionByUser()
            },
            {
                "iconSource": "image://skin/24x24/Outline/settings.svg",
                "text": qsTr("App Settings"),
                "action": () => Workflow.openSettingsScreen()
            },
        ]

        delegate: MenuButtonItem
        {
            width: (parent && parent.width) ?? 0

            text: modelData.text
            textItem.font.pixelSize: 18
            onClicked: modelData.action()
            icon.source: modelData.iconSource
            icon.width: 24
            icon.height: 24
            Component.onCompleted:
            {
                if (modelData.initialize)
                    modelData.initialize(this)
            }
        }
    }

    Component
    {
        id: disconnectButtonCustomArea

        Column
        {
            width: parent.width

            spacing: 12

            Text
            {
                id: loggedAsText

                width: parent.width
                elide: Text.ElideRight

                text: qsTr("Logged in as %1", "%1 is a user name")
                    .arg(windowContext.sessionManager.isCloudSession
                        ? cloudUserProfileWatcher.fullName
                        : (windowContext.mainSystemContext?.userWatcher.userName ?? ""))
                font.pixelSize: 14
                font.weight: 400
                color: ColorTheme.colors.light14
            }

            Text
            {
                id: disconnectText

                width: parent.width
                height: 24
                elide: Text.ElideRight

                text: qsTr("Disconnect")
                font.pixelSize: 16
                font.weight: 500
                color: ColorTheme.colors.red
                verticalAlignment: Text.AlignVCenter
            }
        }
    }
}
