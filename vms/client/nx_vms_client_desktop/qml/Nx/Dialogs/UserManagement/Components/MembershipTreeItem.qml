// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Layouts

import Nx.Core
import Nx.Core.Controls
import Nx.Controls

import nx.vms.client.desktop

Item
{
    id: treeItem

    width: parent ? parent.width : 0
    height: 24

    property url iconSource
    property bool enabled: true
    property int offset: 0
    property alias text: itemText.text

    property bool interactive: true

    readonly property bool isTopLevel: (treeItem.offset ?? 0) === 0
    readonly property bool hovered:
        treeItemMouseArea.containsMouse
        || textMouseArea.containsMouse
        || removeGroupButton.hovered
    readonly property color textColor: (!isTopLevel || !interactive)
        ? ColorTheme.colors.light13
        : hovered
            ? ColorTheme.colors.light4
            : ColorTheme.colors.light7

    signal removeClicked()

    MouseArea
    {
        id: treeItemMouseArea

        enabled: treeItem.interactive && treeItem.isTopLevel && treeItem.enabled
        hoverEnabled: true

        anchors.fill: parent
    }

    Item
    {
        anchors
        {
            left: parent.left
            top: parent.top
            right: parent.right
        }
        height: 24

        RowLayout
        {
            id: rowLayout
            x: treeItem.isTopLevel ? 0 : 20 * (treeItem.offset - 1)
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4

            Image
            {
                Layout.alignment: Qt.AlignVCenter

                width: 20
                height: 20

                source: "image://svg/skin/user_settings/tree_item_linked.svg"
                sourceSize: Qt.size(width, height)

                visible: !treeItem.isTopLevel
            }

            Item
            {
                Layout.alignment: Qt.AlignVCenter

                width: groupImage.width
                height: groupImage.height

                IconImage
                {
                    id: groupImage
                    width: 20
                    height: 20

                    source: treeItem.iconSource
                    sourceSize: Qt.size(width, height)

                    color: treeItem.textColor
                }
            }

            Text
            {
                id: itemText

                Layout.alignment: Qt.AlignVCenter

                textFormat: Text.StyledText

                text: treeItem.isTopLevel
                    ? highlightMatchingText(model.text)
                    : model.text

                font: Qt.font({pixelSize: 14, weight: Font.Medium})
                color: treeItem.textColor

                // Styled text steals hover from underlying MouseArea.
                MouseArea
                {
                    id: textMouseArea

                    enabled: treeItemMouseArea.enabled
                    hoverEnabled: true

                    anchors.fill: itemText
                }
            }

            ImageButton
            {
                id: removeGroupButton
                Layout.alignment: Qt.AlignVCenter
                width: 20
                height: 20

                visible: treeItem.hovered && treeItem.enabled
                hoverEnabled: true

                icon.source: "image://svg/skin/user_settings/tree_item_remove.svg"
                icon.width: width
                icon.height: height

                background: null

                onClicked: removeClicked()
            }
        }
    }
}
