// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

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
                "visible": windowContext.mainSystemContext?.hasViewBookmarksPermission,
                "color": ColorTheme.colors.blue,
                "action": () => { Workflow.openEventSearchScreen(undefined, cameraListModel, false)}
            },
            {
                "objectName": "analyticsSearchMenuButton",
                "iconSource": "image://skin/20x20/Solid/object.svg",
                "text": qsTr("Objects"),
                "visible": windowContext.mainSystemContext?.hasSearchObjectsPermission,
                "color": ColorTheme.colors.light4,
                "action": () => { Workflow.openEventSearchScreen(undefined, cameraListModel, true)}
            }
        ]

        delegate: Control
        {
            width: parent && parent.width
            height: modelData.visible ? implicitHeight : 0
            visible: height > 0
            bottomPadding: 6

            contentItem: MenuButtonItem
            {
                objectName: modelData.objectName

                text: modelData.text
                icon.source: modelData.iconSource
                icon.width: 20
                icon.height: 20
                foregroundColor: modelData.color
                onClicked: modelData.action()
            }
        }
    }
}
