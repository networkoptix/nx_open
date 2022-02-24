// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.11
import QtQuick.Controls 2.4

import Nx 1.0

Button
{
    id: tile

    // It doesn't have menu.
    function closeTileMenu()
    {}

    padding: 0
    leftPadding: 0
    rightPadding: 0

    focusPolicy: Qt.NoFocus

    contentItem: Item
    {
        Image
        {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 22
            width: 54
            height: 32

            fillMode: Image.Stretch

            source: "image://svg/skin/welcome_screen/tile/connect_to_server.svg"
            sourceSize: Qt.size(width, height)
        }

        Label
        {
            anchors.horizontalCenter: parent.horizontalCenter
            y: 58
            height: 24

            font.pixelSize: 17
            color: ColorTheme.colors.light4

            text: qsTr("Connect to Server")
        }
    }

    background: Rectangle
    {
        radius: 2

        border.color: ColorTheme.colors.dark7
        color: tile.hovered ? ColorTheme.colors.dark8 : "transparent"
    }

    onClicked: context.connectToAnotherSystem()
}
