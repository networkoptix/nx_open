// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx
import Nx.Core
import Nx.Controls

import nx.client.desktop

CompactTabButton
{
    height: parent ? parent.height : 0

    underlineOffset: -1
    topPadding: 14
    focusFrameEnabled: false
    secondaryStyle: false

    compact: false

    underlineHeight: 2
    inactiveUnderlineColor: "transparent"
    font: Qt.font({pixelSize: 12, weight: Font.Normal})

    FocusFrame
    {
        anchors.fill: parent
        anchors
        {
            leftMargin: 0
            rightMargin: 0
            topMargin: 1
            bottomMargin: 1
        }
        visible: parent.visualFocus
        color: ColorTheme.highlight
        opacity: 0.5
    }
}
