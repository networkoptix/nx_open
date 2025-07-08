// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx.Core
import Nx.Core.Controls

import nx.vms.client.core
import nx.vms.client.desktop

MenuItem
{
    id: menuItem

    topPadding: 0
    bottomPadding: 0
    leftPadding: 0
    rightPadding: 0

    width: parent ? parent.width : implicitWidth

    visible: action ? action.visible : true

    onTriggered:
    {
        if (menu && !subMenu && menu.hasOwnProperty("triggered"))
            menu.triggered(action, menuItem)
    }

    contentItem: Item
    {
        implicitWidth: implicitWidthLeftPart + shortcutIndent
            + implicitWidthRightPart + rightPadding
        implicitHeight: 24

        opacity: menuItem.enabled ? 1.0 : 0.3

        readonly property int shortcutIndent: action && (action.shortcut !== null) ? 40 : 0
        readonly property int rightPadding: hotkey.visible ? 12 : 28
        readonly property int implicitWidthLeftPart: row.width + row.rightPadding
        readonly property int implicitWidthRightPart: hotkey.visible ? hotkey.width : 0

        Row
        {
            id: row

            Item
            {
                id: iconArea

                width: 28
                height: menuItem.contentItem.implicitHeight

                ColoredImage
                {
                    anchors.centerIn: iconArea
                    width: 20
                    height: 20

                    sourcePath: menuItem.icon.source
                    primaryColor: itemText.color
                    sourceSize: Qt.size(width, height)
                }
            }

            Text
            {
                id: itemText

                height: menuItem.contentItem.implicitHeight

                verticalAlignment: Text.AlignVCenter
                font.pixelSize: FontConfig.normal.pixelSize
                color: menuItem.highlighted ? ColorTheme.colors.brand_contrast : ColorTheme.colors.light4

                text: menuItem.text
            }
        }

        Label
        {
            id: hotkey

            anchors.right: parent.right
            anchors.rightMargin: parent.rightPadding

            height: menuItem.contentItem.implicitHeight

            color: menuItem.highlighted
                ? ColorTheme.colors.brand_contrast
                : ColorTheme.lighter(ColorTheme.colors.light16, 4)

            verticalAlignment: Text.AlignVCenter
            font.pixelSize: FontConfig.normal.pixelSize

            visible: action && (action.shortcut !== null)
            text: (action && action.shortcut) ? NxGlobals.shortcutText(action.shortcut) : ""
        }
    }

    background: Rectangle
    {
        color: menuItem.highlighted ? ColorTheme.colors.brand_core : "transparent"
    }

    arrow: ColoredImage
    {
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        visible: !!menuItem.subMenu

        sourcePath: "image://skin/20x20/Outline/arrow_right.svg"
        sourceSize: Qt.size(20, 20)
        primaryColor: itemText.color
    }

    indicator: null
}
