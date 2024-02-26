// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls

import Nx
import Nx.Controls
import Nx.Core

import nx.vms.client.core
import nx.vms.client.desktop

import "private"

TabButton
{
    id: tabButton

    readonly property bool isCurrent: TabBar.tabBar && TabBar.index === TabBar.tabBar.currentIndex

    readonly property ButtonColors colors: ButtonColors
    {
        normal: ColorTheme.colors.light12
        hovered: ColorTheme.colors.light10
        pressed: normal
    }

    readonly property ButtonColors backgroundColors: ButtonColors
    {
        normal: "transparent"
        hovered: "transparent"
        pressed: "transparent"
    }

    property color highlightColor: ColorTheme.colors.brand_core
    property color highlightBackgroundColor: backgroundColors.normal

    readonly property color color:
    {
        if (isCurrent)
            return highlightColor

        return pressed
            ? colors.pressed
            : (hovered ? colors.hovered : colors.normal)
    }

    readonly property color backgroundColor:
    {
        if (isCurrent)
            return highlightBackgroundColor

        return pressed
            ? backgroundColors.pressed
            : (hovered ? backgroundColors.hovered : backgroundColors.normal)
    }

    property alias underline: underline
    property alias focusFrame: focusFrame

    height: TabBar.tabBar ? TabBar.tabBar.height : implicitHeight

    font.pixelSize: FontConfig.tabBar.pixelSize

    padding: 8
    spacing: 4

    width: Math.ceil(implicitWidth)
    clip: true

    icon.width: 20
    icon.height: 20

    contentItem: Item
    {
        id: content

        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row
        {
            id: row

            anchors.verticalCenter: parent.verticalCenter
            x: Math.max(0, Math.round((parent.width - width) / 2))

            topPadding: 1
            spacing: tabButton.spacing

            IconImage
            {
                id: image

                source: tabButton.icon.source
                sourceSize: Qt.size(tabButton.icon.width, tabButton.icon.height)
                color: tabButton.color
                name: tabButton.icon.name
                anchors.verticalCenter: parent.verticalCenter
            }

            Text
            {
                id: tabText

                text: tabButton.text
                font: tabButton.font
                color: tabButton.color
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    background: Rectangle
    {
        id: backgroundItem

        color: tabButton.backgroundColor

        Rectangle
        {
            id: underline

            x: content.x - 2
            width: content.width + 4
            height: 1
            anchors.bottom: backgroundItem.bottom

            color: tabButton.isCurrent ? tabButton.highlightColor : ColorTheme.colors.dark12
            visible: tabButton.isCurrent || tabButton.hovered
        }

        FocusFrame
        {
            id: focusFrame

            anchors.fill: backgroundItem
            visible: tabButton.visualFocus
        }
    }
}
