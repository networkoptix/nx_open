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

    property Item owner: parent

    property alias labelText: label.text
    property string supportText
    property string errorText

    property int mode: FieldBackground.Mode.Dark
    property bool compactLabelMode: owner.activeFocus || !!owner.text

    function handleKeyPressedEvent(event)
    {
        if (CoreUtils.isCharKeyPressed(event))
            control.errorText = ""
    }

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
    border.color:
    {
        if (owner.errorText)
            return ColorTheme.colors.red_core
        return owner.activeFocus
            ? ColorTheme.colors.brand_core
            : color
    }

    Text
    {
        id: label

        x: owner.leftPadding
        y: compactLabelMode ? 8 : 18
        width: owner.width - x - owner.rightPadding

        opacity: enabled ? 1 : 0.3

        font.pixelSize: compactLabelMode ? 12 : 16
        font.weight: 400
        color: ColorTheme.colors.light16
        elide: Text.ElideRight
    }

    Text
    {
        id: bottomText

        text: control.errorText
            ? control.errorText
            : control.supportText

        x: owner.leftPadding
        y: parent.height + 4
        width: owner.width - 2 * x

        opacity: enabled ? 1 : 0.3

        font.pixelSize: 12
        font.weight: 400
        color: control.errorText
            ? ColorTheme.colors.red_core
            : ColorTheme.colors.light10
        visible: !!text
        elide: Text.ElideRight
    }
}
