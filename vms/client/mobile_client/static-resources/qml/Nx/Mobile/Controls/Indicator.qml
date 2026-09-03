// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Ui

Rectangle
{
    property alias text: textItem.text

    anchors.top: parent.top
    anchors.right: parent.right

    color: ColorTheme.colors.red_attention
    width: text ? Math.max(12, textItem.width) : 6
    height: text ? 12 : 6
    radius: (text ? 12 : 6) / 2

    Text
    {
        id: textItem

        anchors.centerIn: parent

        topPadding: 1
        visible: text

        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter

        color: StyleHints.foregroundColor
        font.pixelSize: 8
        font.weight: Font.Medium
    }
}
