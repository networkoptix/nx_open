// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Layouts 1.15

import QtGraphicalEffects 1.0

import Nx 1.0
import Nx.Controls 1.0

import nx.vms.client.desktop 1.0

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

    readonly property bool isTopLevel: treeItem.offset === 0
    readonly property bool hovered: treeItemMouseArea.containsMouse || removeGroupButton.hovered
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

        Image
        {
            Layout.alignment: Qt.AlignVCenter

            id: groupImage
            width: 20
            height: 20

            source: treeItem.iconSource
            sourceSize: Qt.size(width, height)

            ColorOverlay
            {
                anchors.fill: parent
                source: parent
                color: treeItem.textColor
            }
        }

        Text
        {
            id: itemText

            Layout.alignment: Qt.AlignVCenter
            text: treeItem.isTopLevel ? highlightMatchingText(model.text) : model.text

            font: Qt.font({pixelSize: 14, weight: Font.Medium})
            color: treeItem.textColor
        }

        ImageButton
        {
            id: removeGroupButton
            Layout.alignment: Qt.AlignVCenter
            width: 20
            height: 20

            visible: treeItem.hovered
            hoverEnabled: true

            icon.source: "image://svg/skin/user_settings/tree_item_remove.svg"
            icon.width: width
            icon.height: height

            background: null

            onClicked: removeClicked()
        }
    }
}
