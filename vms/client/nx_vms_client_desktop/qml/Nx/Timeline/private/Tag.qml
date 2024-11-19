// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Controls

Tag
{
    id: root

    signal clicked()

    textColor: "#51ACF6"
    textCapitalization: Font.MixedCase
    color: "transparent"
    border.color: textColor
    border.width: 1

    MouseArea
    {
        anchors.fill: parent
        onClicked: root.clicked()
    }
}
