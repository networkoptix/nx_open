// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4

import Nx 1.0
import Nx.Controls 1.0 as Nx
import nx.vms.client.core 1.0

Button
{
    id: tile

    property var visibilityMenuModel: None
    property bool hideActionEnabled: false

    function closeTileMenu()
    {
        menuButton.closeMenu()
    }

    padding: 0
    leftPadding: 0
    rightPadding: 0

    focusPolicy: Qt.NoFocus

    contentItem: Item
    {
        Image
        {
            id: stateIcon

            anchors.horizontalCenter: parent.horizontalCenter
            y: 22
            width: 54
            height: 32

            fillMode: Image.Stretch

            source: "image://svg/skin/welcome_screen/tile/connect_to_cloud.svg"
            sourceSize: Qt.size(width, height)
        }

        Label
        {
            id: title

            anchors.horizontalCenter: parent.horizontalCenter
            y: 58
            height: 24

            font.pixelSize: 17
            color: ColorTheme.colors.light4

            text: qsTr("Log in to") + " " + Branding.cloudName()
        }

        MenuButton
        {
            id: menuButton

            anchors.right: parent.right
            anchors.rightMargin: 8
            y: 12

            menuComponent: cloudMenuComponent

            Component
            {
                id: cloudMenuComponent

                TileMenu
                {
                    model: tile.visibilityMenuModel
                    hideActionEnabled: tile.hideActionEnabled
                }
            }
        }
    }

    background: Rectangle
    {
        radius: 2

        border.color: ColorTheme.colors.dark7
        color:
        {
            return (tile.hovered && !menuButton.hovered)
                ? ColorTheme.colors.dark8
                : "transparent"
        }
    }

    onClicked: context.loginToCloud()
}
