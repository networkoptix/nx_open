// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

Rectangle
{
    property alias text: text.text

    anchors.top: parent.top
    anchors.right: parent.right

    color: ColorTheme.colors.red_attention
    width: 12
    height: width
    radius: width / 2

    Text
    {
        id: text

        anchors.fill: parent

        topPadding: 1
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter

        color: ColorTheme.colors.light4
        font.pixelSize: 8
        font.weight: Font.Medium
    }
}
