// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Controls

import nx.vms.client.core
import nx.vms.client.desktop

Item
{
    id: placeholder

    property bool shown: false
    property alias icon: placeholderIcon.source
    property alias title: placeholderTitle.text
    property alias description: placeholderDescription.text
    property Action action: null

    property color textColor: ColorTheme.colors.dark10

    Column
    {
        id: column

        padding: 24
        width: placeholder.width
        y: Math.max(0, Math.floor((placeholder.height - height) * 0.4))

        spacing: 16

        opacity: placeholder.shown ? 1.0 : 0.0
        visible: opacity > 0.0

        // Workaround for delayed visibility change on first show.
        onOpacityChanged:
            opacityBehavior.enabled = true

        Behavior on opacity
        {
            id: opacityBehavior
            enabled: false
            NumberAnimation { duration: 100 }
        }

        Image
        {
            id: placeholderIcon

            width: 64
            height: 64
            sourceSize: Qt.size(width, height)
            anchors.horizontalCenter: column.horizontalCenter
        }

        Column
        {
            spacing: 8

            Text
            {
                id: placeholderTitle

                width: column.width - column.leftPadding - column.rightPadding
                color: placeholder.textColor
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                font.pixelSize: FontConfig.large.pixelSize
            }

            Text
            {
                id: placeholderDescription

                width: column.width - column.leftPadding - column.rightPadding
                color: placeholder.textColor
                horizontalAlignment: Text.AlignHCenter
                textFormat: Text.RichText
                wrapMode: Text.Wrap
                font.pixelSize: 12
            }
        }

        Fader
        {
            anchors.horizontalCenter: parent.horizontalCenter
            opaque: placeholder.action ? placeholder.action.visible : false
            visible: opacity > 0 && !!placeholder.action

            contentItem: Button
            {
                action: placeholder.action
            }
        }
    }
}
