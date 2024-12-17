// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls as QC2

import Nx.Core
import Nx.Controls
import Nx.Core.Controls

TextField
{
    id: searchField

    property var menu: null

    property bool darkMode: false
    property color baseColor: darkMode ? ColorTheme.colors.dark3 : ColorTheme.colors.dark5
    property color borderColor: darkMode ? ColorTheme.colors.dark7 : ColorTheme.colors.dark4
    property real iconPadding: 6
    property int spacing: 4

    property alias iconSource: actionButton.icon.source

    placeholderText: qsTr("Search")
    placeholderTextColor: searchField.activeFocus || searchField.hovered
        ? ColorTheme.lighter(ColorTheme.windowText, 1)
        : ColorTheme.windowText

    leftPadding: actionButton.visible ? (actionButton.x + actionButton.width + spacing) : 8
    rightPadding: enabled ? (width - clearButton.x) : 8

    background: Rectangle
    {
        radius: 2

        color:
        {
            if (searchField.hovered)
                return ColorTheme.lighter(baseColor, 1)

            if (searchField.activeFocus)
                return darkMode ? baseColor : ColorTheme.darker(baseColor, 1)

            return baseColor
        }

        border.width: 1
        border.color:
        {
            if (darkMode)
                return borderColor

            if (searchField.hovered)
                return ColorTheme.lighter(borderColor, 1)

            return searchField.activeFocus ? ColorTheme.darker(borderColor, 2) : borderColor
        }
    }

    QC2.Button
    {
        id: actionButton

        enabled: !!menu
        visible: !!icon.source
        width: image.implicitWidth
        height: parent.height - 2
        anchors.left: parent.left
        anchors.leftMargin: searchField.iconPadding
        focusPolicy: Qt.NoFocus
        padding: 0

        icon.source: searchField.menu
            ? "image://skin/24x16/Outline/search_drop.svg"
            : "image://skin/16x16/Outline/search.svg"

        icon.color: searchField.text ? searchField.color : searchField.placeholderTextColor

        icon.width: implicitWidth
        icon.height: implicitHeight

        contentItem: Item
        {
            ColoredImage
            {
                id: image

                anchors.centerIn: parent

                width: implicitWidth
                height: implicitHeight

                sourcePath: actionButton.icon.source
                sourceSize: searchField.menu ? Qt.size(24, 16) : Qt.size(16, 16)
                primaryColor: actionButton.icon.color
            }
        }

        background: Item {}

        onClicked:
        {
            if (menu instanceof Menu)
                menu.popup(actionButton, 0, actionButton.height)
            else if (menu instanceof PlatformMenu)
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

        icon.source: "image://skin/16x16/Outline/close.svg"
        icon.width: 16
        icon.height: 16
        icon.color: searchField.color

        enabled: searchField.text
        opacity: searchField.text ? 1.0 : 0.0
        Behavior on opacity { NumberAnimation { duration: 100 }}

        background: Item {}

        onClicked:
            searchField.clear()
    }

    Connections
    {
        target: menu

        function onAboutToShow() { searchField.cursorVisible = false }
        function onAboutToHide() { searchField.cursorVisible = searchField.activeFocus }
    }

    Keys.onShortcutOverride: (event) =>
    {
        event.accepted = activeFocus && (event.key === Qt.Key_Enter || event.key === Qt.Key_Return)
    }
}
