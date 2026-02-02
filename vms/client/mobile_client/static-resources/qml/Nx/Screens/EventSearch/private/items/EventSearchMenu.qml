// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Controls
import Nx.Core
import Nx.Ui

Item
{
    id: eventSearchMenu

    objectName: "eventSearchMenu"

    readonly property string title: qsTr("Looking for")
    signal menuItemClicked(bool isAnalyticsSearchMode)

    Flow
    {
        id: flow

        anchors.fill: parent
        anchors.margins: 20
        spacing: LayoutController.isTabletLayout ? 12 : 4

        Repeater
        {
            model: [
                /*
                // Uncomment this option when the motion search is ready.
                {
                    "objectName": "motionSearchMenuButton",
                    "iconSource": "image://skin/20x20/Solid/motion.svg",
                    "text": qsTr("Motions"),
                },
                */
                {
                    "objectName": "bookmarksSearchMenuButton",
                    "iconSource": "image://skin/20x20/Solid/bookmark.svg?primary=light4",
                    "text": qsTr("Bookmarks"),
                    "visible": windowContext.mainSystemContext?.hasViewBookmarksPermission,
                    "analyticsSearchMode": false
                },
                {
                    "objectName": "analyticsSearchMenuButton",
                    "iconSource": "image://skin/20x20/Solid/object.svg?primary=light4",
                    "text": qsTr("Objects"),
                    "visible": windowContext.mainSystemContext?.hasSearchObjectsPermission,
                    "analyticsSearchMode": true
                }
            ]

            delegate: Button
            {
                objectName: modelData.objectName

                alignment: LayoutController.isMobile ? Qt.AlignLeft : Qt.AlignCenter
                width: LayoutController.isMobile ? flow.width : 261
                height: LayoutController.isMobile ? 56 : 120
                padding: 0
                leftPadding: 0
                rightPadding: 0
                text: modelData.text
                icon.source: modelData.iconSource
                icon.width: 24
                icon.height: 24
                color: ColorTheme.colors.dark7
                display: LayoutController.isMobile
                    ? AbstractButton.TextBesideIcon
                    : AbstractButton.TextUnderIcon
                visible: modelData.visible

                onClicked: eventSearchMenu.menuItemClicked(modelData.analyticsSearchMode)
            }
        }
    }
}
