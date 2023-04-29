// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.15

import Nx 1.0
import Nx.Core 1.0

Item
{
    id: control

    width: parent.width
    height: innerItem.childrenRect.height

    default property alias data: innerItem.data
    property alias text: label.text

    property real leftSideMargin: 240
    property real rightSideMargin: 240

    property real labelHeight: 28

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

        horizontalAlignment: Qt.AlignRight
        verticalAlignment: Qt.AlignVCenter

        height: control.labelHeight > 0 ? control.labelHeight : implicitHeight

        anchors.top: parent.top
        anchors.bottom: control.labelHeight > 0 ? undefined : parent.bottom
        anchors.right: innerItem.left
        anchors.rightMargin: 8

        font: Qt.font({pixelSize: 14, weight: Font.Normal})
        color: ColorTheme.colors.light16
    }
}
