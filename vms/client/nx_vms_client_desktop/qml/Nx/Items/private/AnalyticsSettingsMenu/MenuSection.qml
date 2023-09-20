// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15
import QtQuick.Controls 2.15

import Nx 1.0
import Nx.Core 1.0

import "."

Column
{
    id: control

    property alias navigationMenu: menuItem.navigationMenu
    property var itemId: this

    property alias text: menuItem.text
    property alias active: menuItem.active
    property alias font: menuItem.font
    property alias selected: menuItem.selected
    property alias indicatorText: indicator.text
    property alias content: menuContent.contentItem
    property alias collapsed: menuContent.collapsed
    property int level: 0
    property bool collapsible: content.implicitHeight > 0
    property bool mainItemVisible: true

    signal clicked()

    MenuItem
    {
        id: menuItem

        itemId: control.itemId
        width: control.width
        level: control.level
        visible: mainItemVisible

        onClicked:
        {
            if (collapsible)
                menuContent.collapsed = false

            control.clicked()
        }

        ArrowButton
        {
            height: parent.height
            x: menuItem.leftPadding - 8 - width / 2
            visible: collapsible
            state: menuContent.collapsed ? "right" : "down"
            icon.lineWidth: control.level === 0 ? 1.5 : 1

            onClicked:
            {
                if (selected && !menuItem.current)
                    menuItem.click()

                menuContent.collapsed = !menuContent.collapsed
            }
        }

        Text
        {
            id: indicator

            font: parent.font
            color: menuContent.collapsed ? ColorTheme.highlight : parent.color

            anchors.right: parent.right
            anchors.baseline: parent.baseline
            anchors.margins: 8
        }
    }

    Control
    {
        id: menuContent

        property bool collapsed: false

        width: parent.width
        height: collapsed ? 0 : implicitHeight
        clip: true
        visible: active
    }
}
