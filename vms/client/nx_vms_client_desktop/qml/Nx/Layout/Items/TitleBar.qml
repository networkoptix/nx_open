// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

Item
{
    property alias titleText: title.text

    property alias backgroundColor: background.color
    property alias backgroundOpacity: background.opacity
    property alias leftContentOpacity: leftContent.opacity
    property alias rightContentOpacity: rightContent.opacity
    property alias titleOpacity: title.opacity

    readonly property alias leftContent: leftContent
    readonly property alias rightContent: rightContent

    width: parent ? parent.width : 120
    height: implicitHeight * sizeScale

    implicitWidth: leftContent.implicitWidth + title.implicitWidth + rightContent.implicitWidth
    implicitHeight: 26

    readonly property real sizeScale: Math.min(width / implicitWidth, 1)

    Rectangle
    {
        id: background
        anchors.fill: parent
        color: ColorTheme.colors.dark1
    }

    Row
    {
        id: leftContent

        height: parent.implicitHeight
        anchors.verticalCenter: parent.verticalCenter
        transformOrigin: Item.Left
        scale: sizeScale
    }

    Text
    {
        id: title

        x: leftContent.width * sizeScale
        anchors.verticalCenter: parent.verticalCenter
        transformOrigin: Item.Left
        scale: sizeScale

        leftPadding: 3
        rightPadding: 3

        font.pixelSize: FontConfig.large.pixelSize
        font.weight: Font.Bold

        color: ColorTheme.brightText
    }

    Row
    {
        id: rightContent

        height: parent.implicitHeight
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        transformOrigin: Item.Right
        scale: sizeScale
    }
}
