// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core
import Nx.Mobile.Controls

Rectangle
{
    id: bar

    property int index: 0
    property int count: 1

    property string labelTemplate: "%1/%2"

    color: ColorTheme.colors.dark10
    height: 52
    width: parent?.width ?? 100

    component NavigationButton: Button
    {
        icon.color: ColorTheme.colors.light10
        opacity: enabled ? 1.0 : 0.3
        background: null
    }

    NavigationButton
    {
        id: leftButton

        anchors.left: parent.left
        anchors.leftMargin: 4
        anchors.verticalCenter: bar.verticalCenter

        icon.source: "image://skin/20x20/Outline/arrow_left.svg"
        enabled: bar.index > 0

        onClicked:
            --bar.index
    }

    Text
    {
        id: label

        anchors.left: leftButton.right
        anchors.right: rightButton.left
        anchors.verticalCenter: bar.verticalCenter

        color: ColorTheme.colors.light16
        font.pixelSize: 14

        text: bar.labelTemplate.arg(bar.index + 1).arg(bar.count)
        horizontalAlignment: Text.AlignHCenter
    }

    NavigationButton
    {
        id: rightButton

        anchors.right: bar.right
        anchors.rightMargin: 4
        anchors.verticalCenter: bar.verticalCenter

        icon.source: "image://skin/20x20/Outline/arrow_right.svg"
        enabled: bar.index < (bar.count - 1)

        onClicked:
            ++bar.index
    }
}
