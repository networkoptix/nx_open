// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0

Item
{
    width: parent.width
    height: innerItem.childrenRect.height

    default property alias data: innerItem.data
    property alias text: label.text

    Item
    {
        id: innerItem

        x: 240
        width: parent.width - 240 * 2

        anchors.horizontalCenter: parent.horizontalCenter
    }

    Text
    {
        id: label
        height: 16
        anchors.top: parent.top
        anchors.topMargin: 4
        anchors.right: innerItem.left
        anchors.rightMargin: 8

        font: Qt.font({pixelSize: 14, weight: Font.Normal})
        color: ColorTheme.colors.light16
    }
}
