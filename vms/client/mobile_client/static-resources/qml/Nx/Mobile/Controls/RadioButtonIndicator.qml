// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

Item
{
    id: indicator

    property bool checked: false

    opacity: enabled ? 1.0 : 0.3

    implicitWidth: 24
    implicitHeight: 24

    Rectangle
    {
        width: 24
        height: 24
        radius: 12

        layer.enabled: indicator.checked && indicator.opacity < 1

        color: indicator.checked
            ? ColorTheme.colors.attention.green
            : ColorTheme.colors.dark11

        border.color: indicator.checked
            ? "transparent"
            : ColorTheme.colors.dark13

        Rectangle
        {
            width: 12
            height: 12
            radius: 6
            anchors.centerIn: parent
            color: ColorTheme.colors.dark1
            visible: indicator.checked
        }
    }
}
