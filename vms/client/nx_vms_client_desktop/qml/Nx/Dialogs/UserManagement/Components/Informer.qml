// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import QtGraphicalEffects 1.0

import Nx 1.0
import Nx.Controls 1.0

Rectangle
{
    id: informer

    height: 78
    color: ColorTheme.colors.red_d4

    property alias text: informerText.text
    property alias buttonText: informerDeleteButton.text
    property alias buttonIcon: informerDeleteButton.icon

    signal clicked()

    Text
    {
        id: informerText

        x: 16
        y: 12

        font: Qt.font({pixelSize: 14, weight: Font.Normal})
        color: ColorTheme.colors.light4
    }

    Button
    {
        id: informerDeleteButton

        visible: control.deleteAvailable

        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12

        leftPadding: 10
        rightPadding: 8
        spacing: 8

        textColor: ColorTheme.colors.light4

        background: Rectangle
        {
            radius: 4
            color: (!informerDeleteButton.pressed && informerDeleteButton.hovered)
                ? Qt.rgba(1, 1, 1, 0.12) //< Hovered.
                : informerDeleteButton.pressed
                    ? Qt.rgba(1, 1, 1, 0.08) //< Pressed.
                    : Qt.rgba(1, 1, 1, 0.1) //< Default.
        }
    }

    ImageButton
    {
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.top: parent.top
        anchors.topMargin: 10

        icon.source: "image://svg/skin/user_settings/cross.svg"
        icon.width: 20
        icon.height: 20
        icon.color: ColorTheme.colors.light4

        onClicked: informer.clicked()
    }

    DropShadow
    {
        id: shadow

        anchors.fill: parent
        z: -1
        verticalOffset: -1
        radius: 3

        color: Qt.rgba(51 / 255, 9 / 255, 9 / 255, 0.4)
        source: informer
    }
}
