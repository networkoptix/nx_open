// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.14
import QtQuick.Controls 2.14

import Nx 1.0

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
        if (menu && menu.hasOwnProperty("triggered"))
            menu.triggered(menuItem, action && action.data)
    }

    contentItem: Item
    {
        implicitWidth: implicitWidthLeftPart + shortcutIndent
            + implicitWidthRightPart + rightPadding
        implicitHeight: 24

        opacity: menuItem.enabled ? 1.0 : 0.3

        readonly property int shortcutIndent: 40
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

                Image
                {
                    anchors.centerIn: iconArea
                    width: 20
                    height: 20

                    source: menuItem.icon.source
                    sourceSize: Qt.size(width, height)
                }
            }

            Text
            {
                height: menuItem.contentItem.implicitHeight

                verticalAlignment: Text.AlignVCenter
                font.pixelSize: 13
                color: hovered ? ColorTheme.highlightContrast : ColorTheme.text

                text: menuItem.text
            }
        }

        Label
        {
            id: hotkey

            anchors.right: parent.right
            anchors.rightMargin: parent.rightPadding

            height: menuItem.contentItem.implicitHeight

            color: menuItem.hovered
                ? ColorTheme.colors.brand_contrast
                : ColorTheme.lighter(ColorTheme.windowText, 4)

            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 13

            visible: action && (action.shortcut !== null)
            text: (action && action.shortcut) ? action.shortcut : ""
        }
    }

    background: Rectangle
    {
        color: hovered ? ColorTheme.highlight : "transparent"
    }

    indicator: null
}
