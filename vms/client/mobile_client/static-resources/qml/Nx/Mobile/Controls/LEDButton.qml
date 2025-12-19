// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

Button
{
    id: control

    property color indicatorColor: checked
        ? ColorTheme.colors.green_attention
        : ColorTheme.colors.dark12

    indicator: Rectangle
    {
        x: control.leftPadding

        anchors.verticalCenter: control.contentItem
            ? control.contentItem.verticalCenter
            : control.verticalCenter

        width: 3
        height: 12
        radius: 1

        color: control.indicatorColor
    }

    spacing: 5
    textIndent: indicator ? (indicator.width + spacing) : 0

    backgroundColor: parameters.colors[state.replace("_checked", "_unchecked")]
    borderColor: "transparent"
}
