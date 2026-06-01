// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Nx.Controls
import Nx.Core

Control
{
    id: control

    signal closeClicked()

    topPadding: 6
    bottomPadding: 6
    leftPadding: 12
    rightPadding: 12

    background: Rectangle
    {
        radius: 6
        color: ColorTheme.transparent(ColorTheme.colors.dark3, 0.5)
    }

    contentItem: RowLayout
    {
        spacing: 4

        Text
        {
            id: title

            text: qsTr("Tap anywhere on video to center view there")
            font.pixelSize: 14
            font.weight: Font.Medium
            color: ColorTheme.colors.light1
        }

        IconButton
        {
            id: closeButton

            compact: true
            icon.source: "image://skin/24x24/Outline/close.svg?primary=light4"
            icon.width: 24
            icon.height: 24

            onClicked: control.closeClicked()
        }
    }
}
