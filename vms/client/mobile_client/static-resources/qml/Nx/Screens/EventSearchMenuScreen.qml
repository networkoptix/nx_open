// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls
import Nx.Core
import Nx.Mobile
import Nx.Ui

Page
{
    id: eventSearchMenuScreen

    objectName: "eventSearchMenuScreen"

    title: qsTr("Looking for")

    leftButtonIcon.source: ""
    customBackHandler: () => {}

    QnCameraListModel
    {
        id: cameraListModel
    }

    ListView
    {
        id: buttonsView

        x: 16
        width: parent.width - x * 2
        height: parent.height
        spacing: 6

        model: [
            /*
            // Uncomment this option when the motion search is ready.
            {
                "objectName": "motionSearchMenuButton",
                "iconSource": "image://skin/20x20/Solid/motion.svg",
                "text": qsTr("Motions"),
                "color": ColorTheme.colors.red,
                "action": () => {}
            },
            */
            {
                "objectName": "bookmarksSearchMenuButton",
                "iconSource": "image://skin/20x20/Solid/bookmark.svg",
                "text": qsTr("Bookmarks"),
                "color": ColorTheme.colors.blue,
                "action": () => { Workflow.openEventSearchScreen(undefined, cameraListModel, false)}
            },
            {
                "objectName": "analyticsSearchMenuButton",
                "iconSource": "image://skin/20x20/Solid/airtransport_solid.svg",
                "text": qsTr("Objects"),
                "color": ColorTheme.colors.light4,
                "action": () => { Workflow.openEventSearchScreen(undefined, cameraListModel, true)}
            }
        ]

        delegate: MenuButtonItem
        {
            objectName: modelData.objectName

            width: parent && parent.width

            text: modelData.text
            icon.source: modelData.iconSource
            icon.width: 20
            icon.height: 20
            foregroundColor: modelData.color
            onClicked: modelData.action()
        }
    }
}
