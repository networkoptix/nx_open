// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import QtQuick.Controls.impl 2.4 as T

import Nx 1.0
import Nx.Controls 1.0 as Nx

Nx.TextField
{
    id: searchField

    property PlatformMenu menu: null

    property color hoveredButtonColor: activeFocus
        ? ColorTheme.shadow
        : ColorTheme.lighter(ColorTheme.shadow, 1)

    property real iconPadding: 1
    property int spacing: 2

    property alias iconSource: actionButton.icon.source

    placeholderText: qsTr("Search")

    leftPadding: actionButton.visible ? (actionButton.x + actionButton.width + spacing) : 8
    rightPadding: enabled ? (width - clearButton.x) : 8

    Button
    {
        id: actionButton

        enabled: !!menu
        visible: !!icon.source
        width: image.implicitWidth
        height: parent.height - 2
        anchors.left: parent.left
        anchors.leftMargin: searchField.iconPadding
        anchors.verticalCenter: parent.verticalCenter
        focusPolicy: Qt.NoFocus
        padding: 0

        icon.color: searchField.text ? searchField.color : searchField.placeholderTextColor

        icon.source: searchField.menu
            ? "image://svg/skin/tree/search_drop.svg"
            : "image://svg/skin/tree/search.svg"

        icon.width: implicitWidth
        icon.height: implicitHeight

        contentItem: Item
        {
            T.IconImage
            {
                id: image

                readonly property real factor:
                    Math.min(implicitWidth / parent.width, implicitHeight / parent.height)

                anchors.centerIn: parent

                width: implicitWidth * factor
                height: implicitHeight * factor

                source: actionButton.icon.source
                sourceSize: searchField.menu ? Qt.size(39, 32) : Qt.size(31, 32)

                color: actionButton.icon.color
            }
        }

        background: Rectangle
        {
            radius: 1
            color: (actionButton.hovered && !actionButton.down) ? hoveredButtonColor : "transparent"
        }

        onClicked:
        {
            if (menu)
                menu.open(actionButton)
        }
    }

    Button
    {
        id: clearButton

        width: height
        height: parent.height - 2
        anchors.right: parent.right
        anchors.rightMargin: 1
        anchors.verticalCenter: parent.verticalCenter
        padding: 0
        focusPolicy: Qt.NoFocus
        icon.source: "qrc:///skin/text_buttons/clear.png"
        icon.color: searchField.color

        enabled: searchField.text
        opacity: searchField.text ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 100 }}

        background: Rectangle
        {
            radius: 1
            color: (clearButton.hovered && !clearButton.down) ? hoveredButtonColor : "transparent"
        }

        onClicked:
            searchField.clear()
    }

    Connections
    {
        target: menu

        function onAboutToShow() { searchField.cursorVisible = false }
        function onAboutToHide() { searchField.cursorVisible = searchField.activeFocus }
    }
}
