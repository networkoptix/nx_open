// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0

Item
{
    id: control

    width: parent.width
    height: innerItem.childrenRect.height

    default property alias data: innerItem.data
    property alias text: label.text

    property real leftSideMargin: 240
    property real rightSideMargin: 240

    property bool textVCenter: false

    Item
    {
        id: innerItem

        anchors
        {
            left: control.left
            leftMargin: control.leftSideMargin
            right: control.right
            rightMargin: control.rightSideMargin
        }
    }

    Text
    {
        id: label
        height: 16
        anchors.top: parent.top
        anchors.topMargin: control.textVCenter ? (control.height / 2 - height / 2) : 4
        anchors.right: innerItem.left
        anchors.rightMargin: 8

        font: Qt.font({pixelSize: 14, weight: Font.Normal})
        color: ColorTheme.colors.light16
    }
}
