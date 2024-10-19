// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick 2.0
import QtQuick.Controls 2.4
import Nx.Core 1.0

Pane
{
    id: control

    property string text
    property bool opened: false
    property alias backgroundColor: backgroundItem.color
    property alias enableAnimation: heightBehavior.enabled

    implicitWidth: parent ? parent.width : background.width
    implicitHeight: text.implicitHeight + control.topPadding + control.bottomPadding;

    topPadding: 12
    bottomPadding: 12
    leftPadding: 16
    rightPadding: 16

    font.pixelSize: 16
    font.weight: Font.DemiBold
    clip: true

    height: opened ? implicitHeight : 0
    Behavior on height
    {
        id: heightBehavior

        NumberAnimation
        {
            duration: 300
            easing.type: Easing.OutCubic
        }
    }

    background: Rectangle
    {
        id: backgroundItem

        implicitWidth: 200
        implicitHeight: 40
        color: ColorTheme.colors.red_core
    }

    contentItem: Text
    {
        id: text

        text: control.text
        font: control.font
        width: control.availableWidth
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        color: ColorTheme.windowText
        elide: Text.ElideRight
        wrapMode: Text.WordWrap
    }
}
