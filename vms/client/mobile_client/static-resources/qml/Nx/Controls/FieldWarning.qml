// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.6
import QtQuick.Controls 2.4
import Nx.Core 1.0

Pane
{
    id: control

    property string text
    property bool opened: false

    implicitWidth: contentItem.implicitWidth + leftPadding + rightPadding
    implicitHeight: contentItem.implicitHeight + topPadding + bottomPadding
    font.pixelSize: 14
    clip: true

    padding: 0
    topPadding: 8
    bottomPadding: 8

    height: opened ? implicitHeight : 0
    Behavior on height
    {
        NumberAnimation
        {
            duration: 300
            easing.type: Easing.OutCubic
        }
    }

    background: null

    contentItem: Text
    {
        text: control.text
        font: control.font
        height: control.implicitHeight
        width: control.availableWidth
        verticalAlignment: Text.AlignVCenter
        color: ColorTheme.colors.red_core
        wrapMode: Text.WordWrap
    }
}
