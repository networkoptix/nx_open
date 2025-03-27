// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

import QtQuick

import Nx.Core

// TODO #ynikitenkov Add "error" functionality support.
Rectangle
{
    id: control

    enum Mode
    {
        Light,
        Middle,
        Dark
    }

    property int mode: FieldBackground.Mode.Dark
    property alias labelText: label.text
    property Item owner: parent
    property bool compactLabelMode: owner.activeFocus || !!owner.text

    opacity: enabled ? 1 : 0.3

    radius: 6

    color:
    {
        switch (mode)
        {
        case FieldBackground.Mode.Light:
            return ColorTheme.colors.dark11
        case FieldBackground.Mode.Middle:
            return ColorTheme.colors.dark9
        case FieldBackground.Mode.Dark:
            return ColorTheme.colors.dark7
        }
    }

    border.width: 2
    border.color: owner.activeFocus
        ? ColorTheme.colors.brand_core
        : color

    Text
    {
        id: label

        x: owner.leftPadding
        y: compactLabelMode ? 8 : 18
        width: owner.width - x - owner.rightPadding

        opacity: enabled ? 1 : 0.3

        font.pixelSize: compactLabelMode ? 12 : 16
        color: ColorTheme.colors.light16
        elide: Text.ElideRight
    }
}
